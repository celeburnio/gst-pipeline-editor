/*
 * gst-pipeline-editor.c
 * Author: Alberto García <alberto.garcia.guillen@gmail.com>
*/


#include "gst-pipeline-editor.h"
#include "gst-pe-config.h"

#include <glib-unix.h>

struct _GstPipelineEditor {
    GstPipelineEditorBase  parent;

    guint               sigint_source;
    guint               sigterm_source;
};


/* Options */
static struct
{
    gboolean version;
} s_options;

static GOptionEntry s_cli_options[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &s_options.version, "Print version and exit", NULL },
};


G_DEFINE_TYPE (GstPipelineEditor, gst_pipeline_editor, G_TYPE_APPLICATION)

static gboolean
on_signal_quit (GstPipelineEditor *app)
{
    g_message("Exiting...");
    g_application_quit(G_APPLICATION(app));
    return G_SOURCE_CONTINUE;
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
    GstPipelineEditor *app = GST_PE_APPLICATION(object);

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
