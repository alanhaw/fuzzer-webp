
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <setjmp.h>
#include <stdlib.h>

/* This test is intended for use with a fuzzer only. */

typedef struct _AnimationStructure {
        GtkWindow               *window;
        GdkPixbufAnimation      *anim;
        GdkPixbufAnimationIter  *anim_iter;
        GtkWidget               *image;
        int                     timerReference;
        int                      delay;
} AnimationStructure;

jmp_buf jmpbuffer;

gboolean
delete_objects(GtkWidget *widget, GdkEvent *event, gpointer data) {
        AnimationStructure *ani = (AnimationStructure *) data;
        if (ani->anim)
                g_object_unref(ani->anim);

        if (ani->timerReference) {
                g_source_remove(ani->timerReference);
                ani->timerReference = 0;
        }

        g_free(ani);
        return FALSE;
}

static gboolean
run_finish_timer(gpointer app) {
        g_application_quit(app);
        return TRUE;
}

static void setup_finish_timer(GtkApplication *app, AnimationStructure *ani) {
        ani->timerReference = g_timeout_add(8000, (GSourceFunc)run_finish_timer, app);
}

static gint command_line_clbk ( GApplication *app,
                                GApplicationCommandLine *cmdline,
                                gint *argc) {
        g_return_val_if_fail ( G_IS_APPLICATION ( app ), 0 );
        g_application_activate(app);
        return 0;
}

static void activate(GtkApplication *app, gpointer user_data) {
        GtkWidget *window;
        GtkWidget *label;
        AnimationStructure *ani = (AnimationStructure *) user_data;

        window = gtk_application_window_new(app);
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        label = gtk_label_new("Test WebP Animation");
        gtk_container_add(GTK_CONTAINER (box), label);

        GtkWidget *image = NULL;

        /*GdkPixbuf *staticPixbuf = NULL;
          staticPixbuf = gdk_pixbuf_animation_get_static_image (ani->anim);
          image = gtk_image_new_from_pixbuf (staticPixbuf);
        */

        image = gtk_image_new_from_animation(ani->anim);
        gtk_container_add(GTK_CONTAINER (box), image);
        gtk_container_add(GTK_CONTAINER (window), box);
        gtk_window_set_title(GTK_WINDOW (window), "Test");
        gtk_window_set_default_size(GTK_WINDOW (window), 500, 500);
        g_signal_connect(G_OBJECT(window),
                         "delete-event", G_CALLBACK(delete_objects), ani);
        setup_finish_timer(app, ani);
        gtk_widget_show_all(window);
} /* end of function activate */

void local_assert(gboolean abool) {
        if (!abool)
                longjmp(jmpbuffer, 1);
        else
                return;
}

void local_abort(void) {
        longjmp(jmpbuffer, 2);
}

gint
main(gint argc, gchar *argv[]) {
        const int str_len = 256;
        GError *error = NULL;
        char file_name[str_len];
        file_name[0] = 0;
        int anErr = 0;

        if ((anErr = setjmp(jmpbuffer)) != 0) {
                goto cleanup;
        }

        if (argc == 2) {
                int alen = strlen(argv[1]);
                if (alen < str_len) {
                        strncpy(file_name, argv[1], alen);
                }
                else
                        goto cleanup;
        }
        else
                goto cleanup;

        gtk_init(&argc, &argv);
        size_t cnt = strlen (file_name);
        if (cnt == (str_len - 1)) {
                local_abort();        // likely truncation of file name.
        } else if (cnt == 0) {
                local_abort();        // TEST_FILE not set.
        }

        /* setup animation. */
        GdkPixbufAnimation *anim = NULL;
        GdkPixbufAnimationIter *anim_iter = NULL;
        anim = gdk_pixbuf_animation_new_from_file(file_name, &error);
        local_assert(anim != NULL);
        gboolean isStatic = gdk_pixbuf_animation_is_static_image(anim);
        if (!isStatic) {
                GtkApplication *app;
                GTimeVal curTime;
                AnimationStructure *ani = g_new0(AnimationStructure, 1);
                ani->anim = anim;
                g_get_current_time(&curTime);
                anim_iter = gdk_pixbuf_animation_get_iter(anim, &curTime);
                local_assert(anim_iter != NULL);
                int delay = gdk_pixbuf_animation_iter_get_delay_time(anim_iter);
                ani->anim_iter = anim_iter;
                ani->delay = delay;
                app = gtk_application_new("com.zyx.www", G_APPLICATION_HANDLES_COMMAND_LINE );
                //app = gtk_application_new(NULL, G_APPLICATION_FLAGS_NONE );
                local_assert(app != NULL);
                g_application_register ( (GApplication*) app, NULL, NULL );
                g_signal_connect(app, "activate", G_CALLBACK(activate), ani);
                g_signal_connect(app, "command-line", G_CALLBACK(command_line_clbk), &argc);
                (void) g_application_run(G_APPLICATION(app), argc, argv);
                g_object_unref(app);
        }

        cleanup:
        return 0;
}
