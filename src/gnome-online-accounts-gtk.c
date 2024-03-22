// GNU General Public License v3.0

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>
#include <goabackend/goabackend.h>

#include "gnome-online-accounts-gtk-resources.h"

#define OA_TYPE_APPLICATION oa_window_get_type ()
G_DECLARE_FINAL_TYPE (OaWindow, oa_window, OA, WINDOW, GtkApplicationWindow)

struct _OaWindow
{
    GtkApplicationWindow parent_instance;

    GoaClient *client;

    GListModel *accounts_model;
    GListModel *providers_model;

    GtkWidget *accounts_frame;
    GtkWidget *accounts_listbox;
    GtkWidget *providers_listbox;

    GtkWidget *offline_label;
    GtkWidget *info_button;
};

G_DEFINE_TYPE (OaWindow, oa_window, GTK_TYPE_APPLICATION_WINDOW)

static void
on_info_button_clicked (GtkButton  *button,
                        OaWindow *window)
{
    g_printerr ("INFO HERE\n");
}

static void
show_account_cb (GoaProvider  *provider,
                 GAsyncResult *res,
                 gpointer      user_data)
{
    OaWindow *window = OA_WINDOW (user_data);
    GError *error = NULL;

    if (!goa_provider_show_account_finish (provider, res, &error))
    {
        if (error->code != GOA_ERROR_DIALOG_DISMISSED)
        {
            g_warning ("Error showing account: %s", error->message);
        }
    }

    g_error_free (error);
}

static void
show_account (OaWindow   *window,
              GoaObject  *object)
{
  GoaProvider *provider;
  GoaAccount *account;
  const gchar *provider_type;

  provider = NULL;

  account = goa_object_get_account (object);
  provider_type = goa_account_get_provider_type (account);
  g_object_unref (account);

  provider = goa_provider_get_for_provider_type (provider_type);

  if (provider != NULL)
    {
      goa_provider_show_account (provider,
                                 window->client,
                                 object,
                                 GTK_WINDOW (window),
                                 NULL,
                                 (GAsyncReadyCallback) show_account_cb,
                                 window);
    }
}

static void
add_account_cb (GoaProvider *provider,
                GAsyncResult *res,
                gpointer     user_data)
{
    OaWindow *window = OA_WINDOW (user_data);
    GoaObject *object;
    GError *error = NULL;

    object = goa_provider_add_account_finish (provider, res, &error);

    if (error != NULL && error->code != GOA_ERROR_DIALOG_DISMISSED)
    {
        g_warning ("Problem adding an account: %s", error->message);
        g_clear_object (&object);
        g_error_free (error);
    }

    if (object == NULL)
    {
        return;
    }

    show_account (window, object);
    g_object_unref (object);
}

static void
add_account (OaWindow    *window,
             GoaProvider *provider)
{
  goa_provider_add_account (provider,
                            window->client,
                            GTK_WINDOW (window),
                            NULL,
                            (GAsyncReadyCallback) add_account_cb,
                            window);
}

static void
on_provider_row_activated (GtkListBox    *box,
                           GtkListBoxRow *activated_row,
                           gpointer       user_data)
{
  OaWindow *window = OA_WINDOW (user_data);
  GoaProvider *provider;
  gint index;

  index = gtk_list_box_row_get_index (activated_row);
  provider = g_list_model_get_item (window->providers_model, index);

  add_account (window, provider);

  g_object_unref (provider);
}

static GtkWidget *
create_provider_widget (GObject *item, gpointer user_data)
{
    GoaProvider *provider = GOA_PROVIDER (item);
    GtkWidget *icon, *label, *box;
    GError *error;
    GIcon *gicon;
    gchar *title = NULL;
    gchar *provider_name = NULL;

    /* The main grid */
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  
    /* The provider icon */
    icon = g_object_new (GTK_TYPE_IMAGE,
                         "margin-start", 6,
                         "margin-end", 6,
                         "margin-top", 6,
                         "margin-bottom", 6,
                         NULL);
  
    error = NULL;
    gicon = goa_provider_get_provider_icon (provider, NULL);

    if (error != NULL)
      {
        g_warning ("Error creating GIcon for account: %s (%s, %d)",
                   error->message,
                   g_quark_to_string (error->domain),
                   error->code);
  
        g_clear_error (&error);
      }
    else
      {
        gtk_image_set_from_gicon (GTK_IMAGE (icon), gicon);
        gtk_image_set_pixel_size (GTK_IMAGE (icon), 48);
      }
  
    gtk_box_append (GTK_BOX (box), icon);
  
    /* The name of the provider */
    provider_name = goa_provider_get_provider_name (provider, NULL);

    title = g_strdup_printf ("<b>%s</b>", provider_name);
    g_free (provider_name);
  
    label = g_object_new (GTK_TYPE_LABEL,
                          "ellipsize", PANGO_ELLIPSIZE_END,
                          "label", title,
                          "xalign", 0.0,
                          "use-markup", TRUE,
                          "hexpand", TRUE,
                          NULL);
    gtk_box_append (GTK_BOX (box), label);
  
    g_clear_pointer (&title, g_free);
    g_clear_object (&gicon);

    return box;
}

static void
get_all_providers_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
   OaWindow *window = OA_WINDOW (user_data);
   GList *providers;
   GList *l;

  providers = NULL;
  if (!goa_provider_get_all_finish (&providers, res, NULL))
    return;

  for (l = providers; l != NULL; l = l->next)
    {
      GoaProvider *provider;
      provider = GOA_PROVIDER (l->data);
      g_list_store_append (G_LIST_STORE (window->providers_model), provider);
    }

  g_list_free_full (providers, g_object_unref);
}

static void
on_account_row_activated (GtkListBox    *box,
                          GtkListBoxRow *activated_row,
                          gpointer       user_data)
{
  OaWindow *window = OA_WINDOW (user_data);
  GoaObject *object;
  gint index;

  index = gtk_list_box_row_get_index (activated_row);
  object = g_list_model_get_item (window->accounts_model, index);

  show_account (window, object);

  g_object_unref (object);
}

static void
on_account_removed (GoaClient *client,
                    GoaObject *object,
                    gpointer   user_data)
{
    OaWindow *window = OA_WINDOW (user_data);
    guint position;

    if (g_list_store_find (G_LIST_STORE (window->accounts_model), G_OBJECT (object), &position))
    {
        g_list_store_remove (G_LIST_STORE (window->accounts_model), position);
    }
    else
    {
        g_warning ("Account removed that was never added!");
    }
}

static void
on_account_changed (GoaClient *client,
                    GoaObject *object,
                    gpointer   user_data)
{
    // TODO: anything needed?
    g_debug ("Account changed");
}

static GtkWidget *
create_account_widget (GObject *item, gpointer user_data)
{
    GoaAccount *account;
    GtkWidget *icon, *label, *box;
    GError *error;
    GIcon *gicon;
    gchar* title = NULL;

    account = goa_object_get_account (GOA_OBJECT (item));
  
    /* The main grid */
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  
    /* The provider icon */
    icon = g_object_new (GTK_TYPE_IMAGE,
                         "margin-start", 6,
                         "margin-end", 6,
                         "margin-top", 6,
                         "margin-bottom", 6,
                         NULL);
  
    error = NULL;
    gicon = g_icon_new_for_string (goa_account_get_provider_icon (account), &error);
    if (error != NULL)
      {
        g_warning ("Error creating GIcon for account: %s (%s, %d)",
                   error->message,
                   g_quark_to_string (error->domain),
                   error->code);
  
        g_clear_error (&error);
      }
    else
      {
        gtk_image_set_from_gicon (GTK_IMAGE (icon), gicon);
        gtk_image_set_pixel_size (GTK_IMAGE (icon), 48);
      }
  
    gtk_box_append (GTK_BOX (box), icon);
  
    /* The name of the provider */
    title = g_strdup_printf ("<b>%s</b>\n<small>%s</small>",
                             goa_account_get_provider_name (account),
                             goa_account_get_presentation_identity (account));
  
    label = g_object_new (GTK_TYPE_LABEL,
                          "ellipsize", PANGO_ELLIPSIZE_END,
                          "label", title,
                          "xalign", 0.0,
                          "use-markup", TRUE,
                          "hexpand", TRUE,
                          NULL);
    gtk_box_append (GTK_BOX (box), label);
  
    /* "Needs attention" icon */
    icon = gtk_image_new_from_icon_name ("dialog-warning-symbolic");
    gtk_image_set_icon_size (GTK_IMAGE (icon), GTK_ICON_SIZE_NORMAL);

    gtk_widget_set_visible (icon, FALSE);
  
    g_object_set (icon, "margin_end", 30, NULL);
    g_object_bind_property (account,
                            "attention-needed",
                            icon,
                            "visible",
                            G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    gtk_box_append (GTK_BOX (box), icon);

    g_clear_pointer (&title, g_free);
    g_clear_object (&gicon);
    g_object_unref (account);

    return box;
}

static void
on_account_added (GoaClient *client,
                  GoaObject *object,
                  gpointer   user_data)
{
    OaWindow *window = OA_WINDOW (user_data);
    g_list_store_append (G_LIST_STORE (window->accounts_model), G_OBJECT (object));
}

static void
populate_accounts (OaWindow *window)
{
    GList *accounts, *l;

    accounts = goa_client_get_accounts (window->client);

    if (accounts == NULL)
    {
        // show_page_nothing_selected (app);
    }
    else
    {
        for (l = accounts; l != NULL; l = l->next)
        {
            g_list_store_append (G_LIST_STORE (window->accounts_model), G_OBJECT (l->data));
        }

        g_list_free_full (accounts, g_object_unref);
    }
}

static void
new_client_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    OaWindow *window = OA_WINDOW (user_data);
    GError *error = NULL;

    window->client = goa_client_new_finish (res, &error);
    if (window->client == NULL) {
        g_critical ("Error creating GoaClient: %s\n", error->message);
        g_application_quit (G_APPLICATION (gtk_window_get_application (GTK_WINDOW (window))));
    }

    g_debug ("GoaClient created\n");

    g_signal_connect (window->client, "account-added", G_CALLBACK (on_account_added), window);
    g_signal_connect (window->client, "account-changed", G_CALLBACK (on_account_changed), window);
    g_signal_connect (window->client, "account-removed", G_CALLBACK (on_account_removed), window);

    populate_accounts (window);
    goa_provider_get_all (get_all_providers_cb, window);
}

static void
oa_window_dispose (GObject *object)
{
    OaWindow *window = OA_WINDOW (object);

    if (window->accounts_model != NULL)
    {
        g_list_store_remove_all (G_LIST_STORE (window->accounts_model));
        g_object_unref (window->accounts_model);
        window->accounts_model = NULL;
    }

    if (window->providers_model != NULL)
    {
        g_list_store_remove_all (G_LIST_STORE (window->providers_model));
        g_object_unref (window->providers_model);
        window->providers_model = NULL;
    }

    g_clear_object (&window->client);

    G_OBJECT_CLASS (oa_window_parent_class)->dispose (object);
}

static void
oa_window_init (OaWindow *window)
{
    g_resources_register (oa_resources_get_resource ());
    gtk_widget_init_template (GTK_WIDGET (window));

    goa_client_new (NULL, (GAsyncReadyCallback) new_client_cb, window);

    window->accounts_model = G_LIST_MODEL (g_list_store_new (GOA_TYPE_OBJECT));
    window->providers_model = G_LIST_MODEL (g_list_store_new (GOA_TYPE_PROVIDER));
    gtk_list_box_bind_model (GTK_LIST_BOX (window->accounts_listbox),
                             window->accounts_model,
                             (GtkListBoxCreateWidgetFunc) create_account_widget,
                             window,
                             NULL);
    gtk_list_box_bind_model (GTK_LIST_BOX (window->providers_listbox),
                             window->providers_model,
                             (GtkListBoxCreateWidgetFunc) create_provider_widget,
                             window,
                             NULL);

    GNetworkMonitor *monitor = g_network_monitor_get_default ();

    g_object_bind_property (monitor, "network-available",
                            window->offline_label, "visible",
                            G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

    g_object_bind_property (monitor, "network-available",
                            window->providers_listbox, "sensitive",
                            G_BINDING_SYNC_CREATE);
}

static void
oa_window_class_init (OaWindowClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = oa_window_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/x/gnome-online-accounts/online-accounts.ui");

  gtk_widget_class_bind_template_child (widget_class, OaWindow, accounts_frame);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, accounts_listbox);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, offline_label);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, providers_listbox);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, info_button);

  gtk_widget_class_bind_template_callback (widget_class, on_account_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_provider_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_info_button_clicked);
}

static OaWindow *
oa_window_new (void)
{
    return g_object_new (oa_window_get_type (), NULL);
}

static void
on_app_activate (GtkApplication *app, gpointer user_data)
{
    OaWindow *window;

    window = oa_window_new ();
    gtk_window_set_application (GTK_WINDOW (window), app);

    gtk_window_present (GTK_WINDOW (window));
}

int main(int argc, char *argv[]) {
    GtkApplication *app;
    gint ret;

    textdomain (GETTEXT_PACKAGE);
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);

    g_set_application_name ("gnome-online-accounts-gtk");
    app = gtk_application_new ("org.x.GnomeOnlineAccountsGtk", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (app, "activate", G_CALLBACK (on_app_activate), NULL);

    ret = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    return ret;
}

