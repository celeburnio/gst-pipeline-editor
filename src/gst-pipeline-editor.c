/*
 * gst-pipeline-editor.c
 * Author: Alberto García <alberto.garcia.guillen@gmail.com>
*/


#include "gst-pipeline-editor.h"
#include "gst-pe-config.h"

#include <glib-unix.h>
#include <stdio.h>
#include <string.h>

struct _GstPipelineEditor {
    GstPipelineEditorBase  parent;

    GstBus *bus;
    GstStateChangeReturn ret;
    GstElement *pipeline;
    GMainLoop *main_loop;
    CustomData data;

    guint               sigint_source;
    guint               sigterm_source;
};

/* Callbacks */
static void cb_message (GstBus *bus, GstMessage *msg, CustomData *data) {

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR: {
        GError *err;
        gchar *debug;

        gst_message_parse_error (msg, &err, &debug);
        g_print ("Error: %s\n", err->message);
        g_error_free (err);
        g_free (debug);

        gst_element_set_state (data->pipeline, GST_STATE_READY);
        g_main_loop_quit (data->loop);
        break;
    }
    case GST_MESSAGE_EOS:
        /* end-of-stream */
        gst_element_set_state (data->pipeline, GST_STATE_READY);
        g_main_loop_quit (data->loop);
        break;
    case GST_MESSAGE_BUFFERING: {
        gint percent = 0;
        /* If the stream is live, we do not care about buffering. */
        if (data->is_live) break;
        gst_message_parse_buffering (msg, &percent);
        g_print ("Buffering (%3d%%)\r", percent);
        /* Wait until buffering is complete before start/resume playing */
        if (percent < 100)
            gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
        else
            gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
        break;
    }
    case GST_MESSAGE_CLOCK_LOST:
        /* Get a new clock */
        gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
        gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
        break;
    default:
        /* Unhandled message */
        break;
    }
}

/* Options */
static struct
{
    gboolean version;
    GStrv    arguments;
} s_options;

static GOptionEntry s_cli_options[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &s_options.version, "Print version and exit.", NULL },
    { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &s_options.arguments, "", "PIPELINE-DESCRIPTION" },
};


G_DEFINE_TYPE (GstPipelineEditor, gst_pipeline_editor, G_TYPE_APPLICATION)

static gboolean
on_signal_quit (GstPipelineEditor *application)
{
    g_message("Exiting...");
    GstPipelineEditor *app = GST_PE_APPLICATION(application);
    gst_element_set_state (app->pipeline, GST_STATE_NULL);
    gst_object_unref (app->pipeline);
    g_main_loop_quit (app->main_loop);
    g_main_loop_unref (app->main_loop);
    gst_object_unref (app->bus);

    g_application_quit(G_APPLICATION(app));
    return G_SOURCE_CONTINUE;
}

/* This function is called when an error message is posted on the bus */
static void error_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
  GError *err;
  gchar *debug_info;

  /* Print error details on the screen */
  gst_message_parse_error (msg, &err, &debug_info);
  g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error (&err);
  g_free (debug_info);

  g_main_loop_quit (data->loop);
}

/* Send seek event to change rate */
static void
send_seek_event (CustomData * data)
{
  gint64 position;
  GstEvent *seek_event;

  /* Obtain the current position, needed for the seek event */
  if (!gst_element_query_position (data->pipeline, GST_FORMAT_TIME, &position)) {
    g_printerr ("Unable to retrieve current position.\n");
    return;
  }

  /* Create the seek event */
  if (data->rate > 0) {
    seek_event =
        gst_event_new_seek (data->rate, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE, GST_SEEK_TYPE_SET,
        position, GST_SEEK_TYPE_END, 0);
  } else {
    seek_event =
        gst_event_new_seek (data->rate, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE, GST_SEEK_TYPE_SET, 0,
        GST_SEEK_TYPE_SET, position);
  }

  if (data->video_sink == NULL) {
    /* If we have not done so, obtain the sink through which we will send the seek events */
    g_object_get (data->pipeline, "video-sink", &data->video_sink, NULL);
  }

  /* Send the event */
  gst_element_send_event (data->video_sink, seek_event);

  g_print ("Current rate: %g\n", data->rate);
}

/* Process keyboard input */
static gboolean
handle_keyboard (GIOChannel * source, GIOCondition cond, CustomData * data)
{
  gchar *str = NULL;

  if (g_io_channel_read_line (source, &str, NULL, NULL,
          NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }

  switch (g_ascii_tolower (str[0])) {
    case 'p':
      data->playing = !data->playing;
      gst_element_set_state (data->pipeline,
          data->playing ? GST_STATE_PLAYING : GST_STATE_PAUSED);
      g_print ("Setting state to %s\n", data->playing ? "PLAYING" : "PAUSE");
      break;
    case 's':
      if (g_ascii_isupper (str[0])) {
        data->rate *= 2.0;
      } else {
        data->rate /= 2.0;
      }
      send_seek_event (data);
      break;
    case 'd':
      data->rate *= -1.0;
      send_seek_event (data);
      break;
    case 'n':
      if (data->video_sink == NULL) {
        /* If we have not done so, obtain the sink through which we will send the step events */
        g_object_get (data->pipeline, "video-sink", &data->video_sink, NULL);
      }

      gst_element_send_event (data->video_sink,
          gst_event_new_step (GST_FORMAT_BUFFERS, 1, ABS (data->rate), TRUE,
              FALSE));
      g_print ("Stepping one frame\n");
      break;
    case 'q':
      g_main_loop_quit (data->loop);
      break;
    default:
      break;
  }

  g_free (str);

  return TRUE;
}

static void
gst_pipeline_editor_constructed (GObject *object)
{
    G_OBJECT_CLASS (gst_pipeline_editor_parent_class)->constructed (object);

    GstPipelineEditor *app = GST_PE_APPLICATION(object);

    /* signals */
    app->sigint_source = g_unix_signal_add(SIGINT,
                                            (GSourceFunc) on_signal_quit,
                                            app);
    app->sigterm_source = g_unix_signal_add(SIGTERM,
                                            (GSourceFunc) on_signal_quit,
                                            app);
}

static void
gst_pipeline_editor_open (GApplication               *application,
                      GFile                     **files,
                      gint                        n_files,
                      G_GNUC_UNUSED const char   *hint)
{
    g_assert (n_files);

    if (n_files > 1)
            g_warning ("Requested opening %i files, opening only the first one", n_files);

    g_autofree char *uri = g_file_get_uri(files[0]);
}


static void
gst_pipeline_editor_startup (GApplication *application)
{
    GstPipelineEditor *app = GST_PE_APPLICATION(application);
    G_APPLICATION_CLASS(gst_pipeline_editor_parent_class)->startup(application);
    GIOChannel *io_stdin;
    const char *uri = NULL;

    /* Print usage map */
    g_print ("USAGE: Choose one of the following options, then press enter:\n"
        " 'P' to toggle between PAUSE and PLAY\n"
        " 'S' to increase playback speed, 's' to decrease playback speed\n"
        " 'D' to toggle playback direction\n"
        " 'N' to move to next frame (in the current direction, better in PAUSE)\n"
        " 'Q' to quit\n");

    /* Add a keyboard watch so we get notified of keystrokes */
#ifdef G_OS_WIN32
    io_stdin = g_io_channel_win32_new_fd (fileno (stdin));
#else
    io_stdin = g_io_channel_unix_new (fileno (stdin));
#endif
    g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc) handle_keyboard, &app->data);

    if (!s_options.arguments) {
        g_printerr ("%s: URL not passed in the command line, exiting\n", g_get_prgname ());
            return;
    } else if (g_strv_length (s_options.arguments) > 1) {
        g_printerr ("%s: Cannot load more than one URL.\n", g_get_prgname ());
        return;
    } else {
        uri = s_options.arguments[0];
        g_printerr ("%s: Launching URI: %s\n", g_get_prgname (), uri);
    }
    /* Build the pipeline */
    app->pipeline = gst_parse_launch (g_strconcat ("playbin uri=", uri, NULL), NULL);
    app->bus = gst_element_get_bus (app->pipeline);

    gst_bus_add_signal_watch (app->bus);
    g_signal_connect (G_OBJECT (app->bus), "message::error", (GCallback)error_cb, &app->data);

    /* Start playing */
    app->ret = gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
    if (app->ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (app->pipeline);
        return;
    } else if (app->ret == GST_STATE_CHANGE_NO_PREROLL) {
        app->data.is_live = TRUE;
    }

    app->data.playing = TRUE;
    app->data.rate = 1.0;
    app->main_loop = g_main_loop_new (NULL, FALSE);
    app->data.loop = app->main_loop;
    app->data.pipeline = app->pipeline;

    gst_bus_add_signal_watch (app->bus);
    g_signal_connect (app->bus, "message", G_CALLBACK (cb_message), &app->data);

    g_main_loop_run (app->main_loop);

    g_application_hold(application);
}


static void
gst_pipeline_editor_shutdown (GApplication *application)
{
    G_APPLICATION_CLASS(gst_pipeline_editor_parent_class)->shutdown(application);
}


static int
gst_pipeline_editor_handle_local_options (GApplication *application,
                                    GVariantDict *options)
{
    if (s_options.version) {
            g_print("%s: %s\n", g_get_prgname(), GST_PE_VERSION_STRING);
            return 0;
    }

    return -1;
}


static void
gst_pipeline_editor_activate (GApplication *application)
{
    (void) application;
}

static void
gst_pipeline_editor_dispose (GObject *object)
{
    G_OBJECT_CLASS(gst_pipeline_editor_parent_class)->dispose(object);
}


static void
gst_pipeline_editor_class_init (GstPipelineEditorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->constructed = gst_pipeline_editor_constructed;
    object_class->dispose = gst_pipeline_editor_dispose;

    GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
    application_class->open = gst_pipeline_editor_open;
    application_class->startup = gst_pipeline_editor_startup;
    application_class->activate = gst_pipeline_editor_activate;
    application_class->shutdown = gst_pipeline_editor_shutdown;
    application_class->handle_local_options = gst_pipeline_editor_handle_local_options;
}


static void
gst_pipeline_editor_init (GstPipelineEditor *application)
{
    g_application_add_main_option_entries(G_APPLICATION(application), s_cli_options);
}


static void*
gst_pipeline_editor_create_instance (G_GNUC_UNUSED void* user_data)
{
    /* Global singleton */
    const GApplicationFlags app_flags =
#if GLIB_CHECK_VERSION(2, 48, 0)
            G_APPLICATION_CAN_OVERRIDE_APP_ID |
#endif // GLIB_CHECK_VERSION
            G_APPLICATION_HANDLES_OPEN ;

    return g_object_new (GST_PE_TYPE_APPLICATION,
                            "application-id", "com.gst-pipeline-editor",
                            "flags",  app_flags,
                            NULL);
}


GstPipelineEditor*
gst_pipeline_editor_get_default (void)
{
    static GOnce create_instance_once = G_ONCE_INIT;
    g_once (&create_instance_once, gst_pipeline_editor_create_instance, NULL);
    g_assert_nonnull (create_instance_once.retval);
    return create_instance_once.retval;
}
