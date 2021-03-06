/*
 * main.c
 * Author: Alberto García <alberto.garcia.guillen@gmail.com>
*/

#include "gst-pipeline-editor.h"
#include <gst/gst.h>

int
main(int argc, char *argv[])
{
    g_autoptr(GApplication) app = G_APPLICATION (gst_pipeline_editor_get_default ());

    /* Initialize GStreamer */
    gst_init (&argc, &argv);

    return g_application_run(app, argc, argv);
}
