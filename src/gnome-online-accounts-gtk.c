// GNU General Public License v3.0

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>
#include <goabackend/goabackend.h>

#include "gnome-online-accounts-gtk-resources.h"

#define OA_TYPE_WINDOW oa_window_get_type ()
G_DECLARE_FINAL_TYPE (OaWindow, oa_window, OA, WINDOW, GtkApplicationWindow)

struct _OaWindow
{
  GtkApplicationWindow parent_instance;

  GoaClient *client;
  GoaObject *active_object;
  GoaObject *removed_object;

  GtkWidget *accounts_frame;
  GtkWidget *accounts_listbox;
  GtkWidget *edit_account_dialog;
  GtkWidget *edit_account_headerbar;
  GtkWidget *new_account_vbox;
  GtkWidget *notification_label;
  GtkWidget *notification_revealer;
  GtkWidget *offline_revealer;
  GtkWidget *providers_listbox;
  GtkWidget *remove_account_button;
  GtkWidget *stack;
  GtkWidget *accounts_vbox;
  GtkWidget *accounts_label;
  GtkWidget *main_menu;

  guint      remove_account_timeout_id;
};

G_DEFINE_TYPE (OaWindow, oa_window, GTK_TYPE_APPLICATION_WINDOW)


static gboolean on_edit_account_dialog_delete_event (OaWindow *window);

static void on_listbox_row_activated (OaWindow    *window,
                                      GtkListBoxRow *activated_row);

static void fill_accounts_listbox (OaWindow *window);

static void on_account_added (GoaClient  *client,
                              GoaObject  *object,
                              gpointer    user_data);

static void on_account_changed (GoaClient  *client,
                                GoaObject  *object,
                                gpointer    user_data);

static void on_account_removed (GoaClient  *client,
                                GoaObject  *object,
                                gpointer    user_data);

static void select_account_by_id (OaWindow    *window,
                                  const gchar   *account_id);

static void get_all_providers_cb (GObject      *source,
                                  GAsyncResult *res,
                                  gpointer      user_data);

static void show_page_account (OaWindow *window,
                               GoaObject  *object);

static void on_remove_button_clicked (OaWindow *window);

static void on_notification_closed (GtkButton  *button,
                                    OaWindow *window);

static void on_undo_button_clicked (GtkButton  *button,
                                    OaWindow *window);

enum {
  PROP_0,
  PROP_PARAMETERS
};


/* From c-c-c list-box-helper.c */
#define MAX_ROWS_VISIBLE 5

void
list_box_update_header_func (GtkListBoxRow *row,
                                GtkListBoxRow *before,
                                gpointer user_data)
{
  GtkWidget *current;

  if (before == NULL)
    {
      gtk_list_box_row_set_header (row, NULL);
      return;
    }

  current = gtk_list_box_row_get_header (row);
  if (current == NULL)
    {
      current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (current);
      gtk_list_box_row_set_header (row, current);
    }
}

void
list_box_adjust_scrolling (GtkListBox *listbox)
{
  GtkWidget *scrolled_window;
  GList *children;
  guint n_rows, num_max_rows;

  scrolled_window = g_object_get_data (G_OBJECT (listbox), "cc-scrolling-scrolled-window");
  if (!scrolled_window)
    return;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));
  n_rows = g_list_length (children);

  num_max_rows = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (listbox), "cc-max-rows-visible"));

  if (n_rows >= num_max_rows)
    {
      gint total_row_height = 0;
      GList *l;
      guint i;

      for (l = children, i = 0; l != NULL && i < num_max_rows; l = l->next, i++) {
        gint row_height;
        gtk_widget_get_preferred_height (GTK_WIDGET (l->data), &row_height, NULL);
        total_row_height += row_height;
      }

      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), total_row_height);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    }
  else
    {
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), -1);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    }

  g_list_free (children);
}

void
list_box_setup_scrolling (GtkListBox *listbox,
                             guint       num_max_rows)
{
  GtkWidget *parent;
  GtkWidget *scrolled_window;

  parent = gtk_widget_get_parent (GTK_WIDGET (listbox));
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolled_window);

  g_object_ref (listbox);
  gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (listbox));
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (listbox));
  g_object_unref (listbox);

  gtk_container_add (GTK_CONTAINER (parent), scrolled_window);

  if (num_max_rows == 0)
    num_max_rows = MAX_ROWS_VISIBLE;

  g_object_set_data (G_OBJECT (listbox), "cc-scrolling-scrolled-window", scrolled_window);
  g_object_set_data (G_OBJECT (listbox), "cc-max-rows-visible", GUINT_TO_POINTER (num_max_rows));
}

/* ---------------------------------------------------------------------------------------------------- */

static void
reset_headerbar (OaWindow *window)
{
  gtk_header_bar_set_title (GTK_HEADER_BAR (window->edit_account_headerbar), NULL);
  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (window->edit_account_headerbar), NULL);
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (window->edit_account_headerbar), TRUE);

  /* Remove any leftover widgets */
  gtk_container_foreach (GTK_CONTAINER (window->edit_account_headerbar),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);

}

/* ---------------------------------------------------------------------------------------------------- */

static void
add_provider_row (OaWindow  *window,
                  GoaProvider *provider)
{
  GIcon *icon;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *row;
  GtkWidget *row_grid;
  gchar *markup;
  gchar *name;

  row = gtk_list_box_row_new ();

  row_grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (row_grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (row_grid), 6);
  gtk_container_add (GTK_CONTAINER (row), row_grid);

  if (provider == NULL)
    {
      g_object_set_data (G_OBJECT (row), "goa-provider", NULL);
      icon = g_themed_icon_new_with_default_fallbacks ("goa-account");
      name = g_strdup (C_("Online Account", "Other"));
    }
  else
    {
      g_object_set_data_full (G_OBJECT (row), "goa-provider", g_object_ref (provider), g_object_unref);
      icon = goa_provider_get_provider_icon (provider, NULL);
      name = goa_provider_get_provider_name (provider, NULL);
    }

  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  gtk_container_add (GTK_CONTAINER (row_grid), image);
  g_object_set (image, "margin", 6, NULL);

  markup = g_strdup_printf ("<b>%s</b>", name);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_container_add (GTK_CONTAINER (row_grid), label);

  gtk_widget_show_all (row);

  gtk_container_add (GTK_CONTAINER (window->providers_listbox), row);

  g_free (markup);
  g_free (name);
  g_object_unref (icon);
}

static gint
sort_providers_func (GtkListBoxRow *a,
                     GtkListBoxRow *b,
                     gpointer       user_data)
{
  GoaProvider *a_provider, *b_provider;
  gboolean a_branded, b_branded;

  a_provider = g_object_get_data (G_OBJECT (a), "goa-provider");
  b_provider = g_object_get_data (G_OBJECT (b), "goa-provider");

  a_branded = (goa_provider_get_provider_features (a_provider) & GOA_PROVIDER_FEATURE_BRANDED) != 0;
  b_branded = (goa_provider_get_provider_features (b_provider) & GOA_PROVIDER_FEATURE_BRANDED) != 0;

  if (a_branded != b_branded)
    {
      if (a_branded)
        return -1;
      else
        return 1;
    }

  return gtk_list_box_row_get_index (b) - gtk_list_box_row_get_index (a);
}

static void
add_account (OaWindow  *window,
             GoaProvider *provider,
             GVariant    *preseed)
{
  GtkWindow *parent;
  GoaObject *object;
  GError *error;

  error = NULL;

  gtk_container_foreach (GTK_CONTAINER (window->new_account_vbox),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);

  reset_headerbar (window);

  /* Move to the new account page */
  gtk_stack_set_visible_child_name (GTK_STACK (window->stack), "new-account");

  parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (window)));
  gtk_window_set_transient_for (GTK_WINDOW (window->edit_account_dialog), parent);

  /* Reset the dialog size */
  gtk_window_resize (GTK_WINDOW (window->edit_account_dialog), 1, 1);

  /* This spins gtk_dialog_run() */
  object = goa_provider_add_account (provider,
                                     window->client,
                                     GTK_DIALOG (window->edit_account_dialog),
                                     GTK_BOX (window->new_account_vbox),
                                     &error);

  if (preseed)
    goa_provider_set_preseed_data (provider, preseed);

  if (object == NULL)
    gtk_widget_hide (window->edit_account_dialog);
  else
    show_page_account (window, object);
}

static void
on_provider_row_activated (OaWindow    *window,
                           GtkListBoxRow *activated_row)
{
  GoaProvider *provider;

  provider = g_object_get_data (G_OBJECT (activated_row), "goa-provider");

  add_account (window, provider, NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

static gint
sort_func (GtkListBoxRow *a,
           GtkListBoxRow *b,
           gpointer       user_data)
{
  GoaObject *a_obj, *b_obj;
  GoaAccount *a_account, *b_account;

  a_obj = g_object_get_data (G_OBJECT (a), "goa-object");
  a_account = goa_object_peek_account (a_obj);

  b_obj = g_object_get_data (G_OBJECT (b), "goa-object");
  b_account = goa_object_peek_account (b_obj);

  return g_strcmp0 (goa_account_get_id (a_account), goa_account_get_id (b_account));
}

static void
command_add (OaWindow *window,
             GVariant   *parameters)
{
  GVariant *v, *preseed = NULL;
  GoaProvider *provider = NULL;
  const gchar *provider_name = NULL;

  g_assert (window != NULL);
  g_assert (parameters != NULL);

  switch (g_variant_n_children (parameters))
    {
      case 3:
        g_variant_get_child (parameters, 2, "v", &preseed);
      case 2:
        g_variant_get_child (parameters, 1, "v", &v);
        if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
          provider_name = g_variant_get_string (v, NULL);
        else
          g_warning ("Wrong type for the second argument (provider name) GVariant, expected 's' but got '%s'",
                     (gchar *)g_variant_get_type (v));
        g_variant_unref (v);
        break;
      default:
        g_warning ("Unexpected parameters found, ignore request");
        goto out;
    }

  if (provider_name != NULL)
    {
      provider = goa_provider_get_for_provider_type (provider_name);
      if (provider == NULL)
        {
          g_warning ("Unable to get a provider for type '%s'", provider_name);
          goto out;
        }

      add_account (window, provider, preseed);
    }

out:
  g_clear_object (&provider);
  g_clear_pointer (&preseed, g_variant_unref);
}

static void
oa_window_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
  switch (property_id)
    {
      case PROP_PARAMETERS:
        {
          GVariant *parameters, *v;
          const gchar *first_arg = NULL;

          parameters = g_value_get_variant (value);
          if (parameters == NULL)
            return;

          if (g_variant_n_children (parameters) > 0)
            {
                g_variant_get_child (parameters, 0, "v", &v);
                if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
                  first_arg = g_variant_get_string (v, NULL);
                else
                  g_warning ("Wrong type for the second argument GVariant, expected 's' but got '%s'",
                             (gchar *)g_variant_get_type (v));
                g_variant_unref (v);
            }

          if (g_strcmp0 (first_arg, "add") == 0)
            command_add (OA_WINDOW (object), parameters);
          else if (first_arg != NULL)
            select_account_by_id (OA_WINDOW (object), first_arg);

          return;
        }
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
oa_window_dispose (GObject *object)
{
  OaWindow *window = OA_WINDOW (object);

  /* Must be destroyed in dispose, not finalize. */
  g_clear_pointer (&window->edit_account_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (oa_window_parent_class)->dispose (object);
}

static void
oa_window_finalize (GObject *object)
{
  OaWindow *window = OA_WINDOW (object);

  g_clear_object (&window->client);

  G_OBJECT_CLASS (oa_window_parent_class)->finalize (object);
}

static void
set_accounts_label_visibility (OaWindow *window) {
  GList *children;
  int n_rows;
  children = gtk_container_get_children (GTK_CONTAINER (window->accounts_listbox));
  n_rows = g_list_length (children);
  if (n_rows > 0) {
    gtk_widget_show (window->accounts_label);
  }
  else {
    gtk_widget_hide (window->accounts_label);
  }
  g_list_free (children);
}

static void
show_about_dialog (GtkMenuItem *item, gpointer     user_data)
{
  GtkAboutDialog *dialog;

  dialog = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());

  gtk_about_dialog_set_program_name (dialog, "GNOME Online Accounts GTK");
  gtk_about_dialog_set_version (dialog, VERSION);
  gtk_about_dialog_set_license_type (dialog, GTK_LICENSE_GPL_3_0);
  gtk_about_dialog_set_website (dialog, "https://www.github.com/xapp-project/gnome-online-accounts-gtk");
  gtk_about_dialog_set_logo_icon_name (dialog, "gnome-online-accounts-gtk");
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
menu_quit (GtkMenuItem *item, gpointer     user_data)
{
  g_application_quit (G_APPLICATION (gtk_window_get_application (GTK_WINDOW (user_data))));
}

static void
oa_window_init (OaWindow *window)
{
  GError *error;
  GNetworkMonitor *monitor;
  GtkWidget *menu_item;
  GtkWidget *image;
  GIcon *icon;

  g_resources_register (oa_resources_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (window));

  gtk_list_box_set_header_func (GTK_LIST_BOX (window->accounts_listbox),
                                list_box_update_header_func,
                                NULL,
                                NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (window->accounts_listbox),
                              sort_func,
                              window,
                              NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (window->providers_listbox),
                                list_box_update_header_func,
                                NULL,
                                NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (window->providers_listbox),
                              sort_providers_func,
                              window,
                              NULL);

  monitor = g_network_monitor_get_default();

  g_object_bind_property (monitor, "network-available",
                          window->offline_revealer, "reveal-child",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (monitor, "network-available",
                          window->providers_listbox, "sensitive",
                          G_BINDING_SYNC_CREATE);

  /* TODO: probably want to avoid _sync() ... */
  error = NULL;
  window->client = goa_client_new_sync (NULL /* GCancellable */, &error);
  if (window->client == NULL)
    {
      g_warning ("Error getting a GoaClient: %s (%s, %d)",
                 error->message, g_quark_to_string (error->domain), error->code);
      gtk_widget_set_sensitive (GTK_WIDGET (window), FALSE);
      g_error_free (error);
      return;
    }

  g_signal_connect (window->client,
                    "account-added",
                    G_CALLBACK (on_account_added),
                    window);

  g_signal_connect (window->client,
                    "account-changed",
                    G_CALLBACK (on_account_changed),
                    window);

  g_signal_connect (window->client,
                    "account-removed",
                    G_CALLBACK (on_account_removed),
                    window);

  fill_accounts_listbox (window);
  goa_provider_get_all (get_all_providers_cb, window);

  set_accounts_label_visibility (window);

  icon = g_content_type_get_symbolic_icon ("help-about-symbolic");
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
  g_object_unref (icon);

  menu_item = gtk_image_menu_item_new_with_label (_("About"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
  g_signal_connect (menu_item, "activate",
        G_CALLBACK (show_about_dialog),
        window);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (window->main_menu), menu_item);

  menu_item = gtk_image_menu_item_new_with_label (_("Quit"));
  g_signal_connect (menu_item, "activate",
        G_CALLBACK (menu_quit),
        window);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (window->main_menu), menu_item);

  gtk_widget_show (GTK_WIDGET (window));
}

static void
oa_window_class_init (OaWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = oa_window_set_property;
  object_class->finalize = oa_window_finalize;
  object_class->dispose = oa_window_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/x/gnome-online-accounts-gtk/online-accounts.ui");

  gtk_widget_class_bind_template_child (widget_class, OaWindow, accounts_frame);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, accounts_listbox);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, accounts_vbox);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, edit_account_dialog);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, edit_account_headerbar);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, new_account_vbox);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, notification_label);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, offline_revealer);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, providers_listbox);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, remove_account_button);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, stack);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, accounts_label);
  gtk_widget_class_bind_template_child (widget_class, OaWindow, main_menu);

  gtk_widget_class_bind_template_callback (widget_class, on_edit_account_dialog_delete_event);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_notification_closed);
  gtk_widget_class_bind_template_callback (widget_class, on_provider_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_undo_button_clicked);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
show_page_nothing_selected (OaWindow *window)
{
}

static void
show_page_account (OaWindow  *window,
                   GoaObject *object)
{
  GtkWindow *parent;
  GList *children;
  GList *l;
  GoaProvider *provider;
  GoaAccount *account;
  gboolean is_locked;
  const gchar *provider_name;
  const gchar *provider_type;
  gchar *title;

  provider = NULL;

  window->active_object = object;
  reset_headerbar (window);

  /* Move to the account editor page */
  gtk_stack_set_visible_child_name (GTK_STACK (window->stack), "editor");

  /* Out with the old */
  children = gtk_container_get_children (GTK_CONTAINER (window->accounts_vbox));
  for (l = children; l != NULL; l = l->next)
    gtk_container_remove (GTK_CONTAINER (window->accounts_vbox), GTK_WIDGET (l->data));
  g_list_free (children);

  account = goa_object_peek_account (object);

  is_locked = goa_account_get_is_locked (account);
  gtk_widget_set_visible (window->remove_account_button, !is_locked);

  provider_type = goa_account_get_provider_type (account);
  provider = goa_provider_get_for_provider_type (provider_type);

  if (provider != NULL)
    {
      goa_provider_show_account (provider,
                                 window->client,
                                 object,
                                 GTK_BOX (window->accounts_vbox),
                                 NULL,
                                 NULL);
    }

  provider_name = goa_account_get_provider_name (account);
  /* translators: This is the title of the "Show Account" dialog. The
   * %s is the name of the provider. e.g., 'Google'. */
  title = g_strdup_printf (_("%s Account"), provider_name);
  gtk_header_bar_set_title (GTK_HEADER_BAR (window->edit_account_headerbar), title);
  g_free (title);

  parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (window)));
  gtk_window_set_transient_for (GTK_WINDOW (window->edit_account_dialog), parent);

  /* Reset the dialog size */
  gtk_window_resize (GTK_WINDOW (window->edit_account_dialog), 1, 1);

  gtk_widget_show_all (window->accounts_vbox);
  gtk_widget_show (window->edit_account_dialog);

  g_clear_object (&provider);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
select_account_by_id (OaWindow    *window,
                      const gchar *account_id)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (window->accounts_listbox));

  for (l = children; l != NULL; l = l->next)
    {
      GoaAccount *account;
      GoaObject *row_object;

      row_object = g_object_get_data (l->data, "goa-object");
      account = goa_object_peek_account (row_object);

      if (g_strcmp0 (goa_account_get_id (account), account_id) == 0)
        {
          show_page_account (window, row_object);
          break;
        }
    }

  g_list_free (children);
}

static gboolean
on_edit_account_dialog_delete_event (OaWindow *window)
{
  window->active_object = NULL;
  gtk_widget_hide (window->edit_account_dialog);
  return TRUE;
}

static void
on_listbox_row_activated (OaWindow    *window,
                          GtkListBoxRow *activated_row)
{
  GoaObject *object;

  object = g_object_get_data (G_OBJECT (activated_row), "goa-object");
  show_page_account (window, object);
}

static void
fill_accounts_listbox (OaWindow *window)
{
  GList *accounts, *l;

  accounts = goa_client_get_accounts (window->client);

  if (accounts == NULL)
    {
      show_page_nothing_selected (window);
    }
  else
    {
      for (l = accounts; l != NULL; l = l->next) {
        on_account_added (window->client, l->data, window);
      }
    }

  g_list_free_full (accounts, g_object_unref);
}

/* ---------------------------------------------------------------------------------------------------- */

typedef void (*RowForAccountCallback) (OaWindow *window, GtkWidget *row, GList *other_rows);

static void
hide_row_for_account (OaWindow *window, GtkWidget *row, GList *other_rows)
{
  gtk_widget_hide (row);
  gtk_widget_set_visible (window->accounts_frame, other_rows != NULL);
}

static void
remove_row_for_account (OaWindow *window, GtkWidget *row, GList *other_rows)
{
  gtk_widget_destroy (row);
  gtk_widget_set_visible (window->accounts_frame, other_rows != NULL);
}

static void
show_row_for_account (OaWindow *window, GtkWidget *row, GList *other_rows)
{
  gtk_widget_show (row);
  gtk_widget_show (window->accounts_frame);
}

static void
modify_row_for_account (OaWindow *window,
                        GoaObject *object,
                        RowForAccountCallback callback)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (window->accounts_listbox));

  for (l = children; l != NULL; l = l->next)
    {
      GoaObject *row_object;

      row_object = g_object_get_data (G_OBJECT (l->data), "goa-object");
      if (row_object == object)
        {
          GtkWidget *row = GTK_WIDGET (l->data);

          children = g_list_remove_link (children, l);
          callback (window, row, children);
          g_list_free (l);
          break;
        }
    }

  g_list_free (children);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_account_added (GoaClient *client,
                  GoaObject *object,
                  gpointer   user_data)
{
  OaWindow *window = user_data;
  GtkWidget *row, *icon, *label, *box;
  GoaAccount *account;
  GError *error;
  GIcon *gicon;
  gchar* title = NULL;

  account = goa_object_peek_account (object);

  /* The main grid */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_show (box);

  /* The provider icon */
  icon = gtk_image_new ();

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
      gtk_image_set_from_gicon (GTK_IMAGE (icon), gicon, GTK_ICON_SIZE_DIALOG);
    }

  g_object_set (icon, "margin", 6, NULL);

  gtk_container_add (GTK_CONTAINER (box), icon);

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
  gtk_container_add (GTK_CONTAINER (box), label);

  /* "Needs attention" icon */
  icon = gtk_image_new_from_icon_name ("dialog-warning-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_no_show_all (icon, TRUE);
  g_object_set (icon, "margin_end", 30, NULL);
  g_object_bind_property (goa_object_peek_account (object),
                          "attention-needed",
                          icon,
                          "visible",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box), icon);

  /* The row */
  row = gtk_list_box_row_new ();
  g_object_set_data (G_OBJECT (row), "goa-object", object);
  gtk_container_add (GTK_CONTAINER (row), box);

  /* Add to the listbox */
  gtk_container_add (GTK_CONTAINER (window->accounts_listbox), row);
  gtk_widget_show_all (row);
  gtk_widget_show (window->accounts_frame);

  g_clear_pointer (&title, g_free);
  g_clear_object (&gicon);

  set_accounts_label_visibility (window);
}

static void
on_account_changed (GoaClient  *client,
                    GoaObject  *object,
                    gpointer    user_data)
{
  OaWindow *window = OA_WINDOW (user_data);

  if (window->active_object != object)
    return;

  show_page_account (window, window->active_object);
}

static void
on_account_removed (GoaClient *client,
                    GoaObject *object,
                    gpointer   user_data)
{
  OaWindow *window = user_data;
  modify_row_for_account (window, object, remove_row_for_account);
  set_accounts_label_visibility (window);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
get_all_providers_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  OaWindow *window = user_data;
  GList *providers;
  GList *l;

  providers = NULL;
  if (!goa_provider_get_all_finish (&providers, res, NULL))
    return;

  for (l = providers; l != NULL; l = l->next)
    {
      GoaProvider *provider;
      provider = GOA_PROVIDER (l->data);

      add_provider_row (window, provider);
    }

  g_list_free_full (providers, g_object_unref);
}


/* ---------------------------------------------------------------------------------------------------- */

static void
cancel_notification_timeout (OaWindow *window)
{
  if (window->remove_account_timeout_id == 0)
    return;

  g_source_remove (window->remove_account_timeout_id);

  window->remove_account_timeout_id = 0;
}

static void
remove_account_cb (GoaAccount    *account,
                   GAsyncResult  *res,
                   gpointer       user_data)
{
  OaWindow *window = OA_WINDOW (user_data);
  GError *error;

  error = NULL;
  if (!goa_account_call_remove_finish (account, res, &error))
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (window))),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Error removing account"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s",
                                                error->message);
      gtk_widget_show_all (dialog);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }
  g_object_unref (window);
}

static void
on_notification_closed (GtkButton  *button,
                        OaWindow *window)
{
  goa_account_call_remove (goa_object_peek_account (window->removed_object),
                           NULL, /* GCancellable */
                           (GAsyncReadyCallback) remove_account_cb,
                           g_object_ref (window));

  gtk_revealer_set_reveal_child (GTK_REVEALER (window->notification_revealer), FALSE);

  cancel_notification_timeout (window);
  window->removed_object = NULL;
}

static void
on_undo_button_clicked (GtkButton  *button,
                        OaWindow *window)
{
  /* Simply show the account row and hide the notification */
  modify_row_for_account (window, window->removed_object, show_row_for_account);
  gtk_revealer_set_reveal_child (GTK_REVEALER (window->notification_revealer), FALSE);

  cancel_notification_timeout (window);
  window->removed_object = NULL;
}

static gboolean
on_remove_account_timeout (gpointer user_data)
{
  on_notification_closed (NULL, user_data);
  return G_SOURCE_REMOVE;
}

static void
on_remove_button_clicked (OaWindow *window)
{
  GoaAccount *account;
  gchar *label;

  if (window->active_object == NULL)
    return;

  if (window->removed_object != NULL)
    on_notification_closed (NULL, window);

  window->removed_object = window->active_object;
  window->active_object = NULL;

  account = goa_object_peek_account (window->removed_object);
  /* Translators: The %s is the username (eg., debarshi.ray@gmail.com
   * or rishi).
   */
  label = g_strdup_printf (_("<b>%s</b> removed"), goa_account_get_presentation_identity (account));
  gtk_label_set_markup (GTK_LABEL (window->notification_label), label);
  gtk_revealer_set_reveal_child (GTK_REVEALER (window->notification_revealer), TRUE);

  modify_row_for_account (window, window->removed_object, hide_row_for_account);
  gtk_widget_hide (window->edit_account_dialog);

  window->remove_account_timeout_id = g_timeout_add_seconds (10, on_remove_account_timeout, window);

  g_free (label);
}

static OaWindow *
oa_window_new (void)
{
    return g_object_new (oa_window_get_type (), NULL);
}

static void
on_app_activate (GtkApplication *app, gpointer user_data)
{
    static OaWindow *window = NULL;

    if (window == NULL)
    {
        window = oa_window_new ();
        gtk_window_set_application (GTK_WINDOW (window), app);
    }

    gtk_window_present (GTK_WINDOW (window));
}

// Bookworm has 2.74.6 currently, Ubuntu 20.04 has 2.72.4
#if !GLIB_CHECK_VERSION (2, 74, 0)
#define G_APPLICATION_DEFAULT_FLAGS G_APPLICATION_FLAGS_NONE
#endif

int main(int argc, char *argv[]) {
    GtkApplication *app;
    gint ret;

    if (argc > 1 && g_strcmp0 (argv[1], "--version") == 0)
    {
        g_print ("gnome-online-accounts-gtk %s\n", VERSION);
        return 0;
    }

    textdomain (GETTEXT_PACKAGE);
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);

    g_set_application_name ("gnome-online-accounts-gtk");
    app = gtk_application_new ("org.x.GnomeOnlineAccountsGtk", G_APPLICATION_DEFAULT_FLAGS);

    g_application_set_option_context_summary (G_APPLICATION (app), "gnome-online-accounts-gtk: A GTK Frontend for GNOME Online Accounts");

    g_signal_connect (app, "activate", G_CALLBACK (on_app_activate), NULL);

    ret = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    return ret;
}
