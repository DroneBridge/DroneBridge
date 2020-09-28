/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2020 Wolfgang Christl
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */


#include <gtk/gtk.h>

void on_main_window_destroy() {
    gtk_main_quit();
}

void get_all_ui_elements(GtkBuilder *pBuilder) {

}

int main (int argc, char *argv[])
{
    GtkWidget *window;
    GError *err = NULL;
    GtkBuilder *builder;

    /* Initialize the GTK+ and all of its supporting libraries. */
    gtk_init (&argc, &argv);
    builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "splash_ui.glade", &err);
    if (err != NULL) {
        fprintf(stderr, "Unable to read file: %s\n", err->message);
        g_error_free(err);
        return 1;
    }

    /* Create a new window, give it a title and display it to the user. */
    window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    g_signal_connect(window, "destroy", G_CALLBACK(on_main_window_destroy), NULL);
    get_all_ui_elements(builder);
    g_object_unref(builder);

    gtk_widget_show(window);
    gtk_main();
    return 0;
}