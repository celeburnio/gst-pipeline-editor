/*
 * gst-pipeline-editor.h
 * Author: Alberto García <alberto.garcia.guillen@gmail.com>
*/

#pragma once
#include <gio/gio.h>
#include <gst/gst.h>

G_BEGIN_DECLS

typedef GApplication GstPipelineEditorBase;
typedef GApplicationClass GstPipelineEditorBaseClass;

typedef struct _CustomData {
  gboolean is_live;
  GstElement *pipeline;
  GMainLoop *loop;
  GstElement *video_sink;

  gboolean playing;             /* Playing or Paused */
  gdouble rate;                 /* Current playback rate (can be negative) */
} CustomData;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstPipelineEditorBase, g_object_unref)

#define GST_PE_TYPE_APPLICATION (gst_pipeline_editor_get_type ())

G_DECLARE_FINAL_TYPE (GstPipelineEditor, gst_pipeline_editor, GST_PE, APPLICATION, GstPipelineEditorBase)

struct _GstPipelineEditorClass
{
    GstPipelineEditorBaseClass parent_class;
};

GstPipelineEditor* gst_pipeline_editor_get_default (void);
