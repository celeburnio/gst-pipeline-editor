/*
 * gst-pipeline-editor.h
 * Author: Alberto García <alberto.garcia.guillen@gmail.com>
*/

#pragma once
#include <gio/gio.h>

G_BEGIN_DECLS

typedef GApplication GstPipelineEditorBase;
typedef GApplicationClass GstPipelineEditorBaseClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstPipelineEditorBase, g_object_unref)

#define GST_PE_TYPE_APPLICATION (gst_pipeline_editor_get_type ())

G_DECLARE_FINAL_TYPE (GstPipelineEditor, gst_pipeline_editor, GST_PE, APPLICATION, GstPipelineEditorBase)

struct _GstPipelineEditorClass
{
    GstPipelineEditorBaseClass parent_class;
};

GstPipelineEditor* gst_pipeline_editor_get_default (void);
