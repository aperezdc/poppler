/*
 * Copyright (C) 2013-2014 Igalia S.L.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <string.h>

#include "text.h"
#include "utils.h"

typedef struct {
  GtkWidget     *view;
  GtkTreeStore  *store;
  GdkPixbuf     *checkerboard;
  GtkWidget     *page_num;
  GtkWidget     *bounding_box;
  GtkWidget     *padding;
  GtkWidget     *type_value;
  GtkWidget     *lang_value;
  GtkWidget     *abbr_value;
  GtkWidget     *id_value;
  GtkWidget     *title_value;
  GtkWidget     *alt_text;
  GtkWidget     *actual_text;
  GtkWidget     *placement;
  GtkWidget     *writing_mode;
  GtkWidget     *text_align;
  GtkWidget     *block_align;
  GtkWidget     *inline_align;
  GtkWidget     *line_height_shift;
  GtkWidget     *text_decoration;
  GtkWidget     *text_decoration_color;
  GtkWidget     *text_decoration_thickness;
  GtkWidget     *ruby_align;
  GtkWidget     *ruby_position;
  GtkWidget     *glyph_orientation;
  GtkWidget     *list_numbering;
  GtkWidget     *form_role;
  GtkWidget     *form_state;
  GtkWidget     *form_description;
  GtkWidget     *table_scope;
  GtkWidget     *space_before_after;
  GtkWidget     *indent_start_end_text;
  GtkWidget     *width_height;
  GtkWidget     *colors[2];
  GtkWidget     *border_style;
  GtkWidget     *border_thickness;
  GtkWidget     *border_colors[4];
  GtkWidget     *column_count;
  GtkWidget     *column_widths;
  GtkWidget     *column_gaps;
  GtkTextBuffer *text_buffer;
} PgdTaggedStructDemo;


static void
pgd_taggedstruct_free (PgdTaggedStructDemo *demo)
{
  if (!demo)
    return;

  if (demo->checkerboard)
    {
      g_object_unref (demo->checkerboard);
      demo->checkerboard = NULL;
    }

  if (demo->store)
    {
      g_object_unref (demo->store);
      demo->store = NULL;
    }

  g_free (demo);
}


static GdkPixbuf *
pgd_pixbuf_new_checkerboard (void)
{
  GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                      TRUE, 8,
                                      64, 16);

  guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
  gint stride = gdk_pixbuf_get_rowstride (pixbuf);
  gint height = gdk_pixbuf_get_height (pixbuf);
  gint width = gdk_pixbuf_get_width (pixbuf);

  gint x, y;

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          guchar *pixel = pixels + y * stride + x * 4;
          pixel[0] = pixel[1] = pixel[2] = 0x88;
          pixel[3] = ((y / 8 + x / 8) % 2 == 0) ? 0x00 : 0xFF;
        }
    }

  return pixbuf;
}


static GtkWidget *
pgd_new_color_image (GtkWidget **imageptr, GdkPixbuf *pixbuf)
{
  GtkWidget *frame = gtk_frame_new (NULL);
  GtkWidget *image = gtk_image_new ();

  if (pixbuf != NULL)
    gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

  gtk_container_add (GTK_CONTAINER (frame), image);
  *imageptr = image;

  return frame;
}


static GtkWidget *
pgd_new_color_image_box (GtkWidget **images, guint n_images, GdkPixbuf *pixbuf)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  while (n_images--)
    {
      GtkWidget *w = pgd_new_color_image (images, pixbuf);
      gtk_container_add (GTK_CONTAINER (box), w);
      images++;
    }

  return box;
}


static void
populate_store_aux (GtkTreeStore *store, GtkTreeIter *parent, PopplerStructureElementIter *iter)
{
  do
    {
      PopplerStructureElementIter *child = poppler_structure_element_iter_get_child (iter);
      PopplerStructureElement *element = poppler_structure_element_iter_get_element (iter);
      GEnumClass *enum_class = G_ENUM_CLASS (g_type_class_ref (POPPLER_TYPE_STRUCTURE_ELEMENT_KIND));
      GEnumValue *enum_value = g_enum_get_value (enum_class, poppler_structure_element_get_kind (element));
      GtkTreeIter pos;

      gtk_tree_store_append (store, &pos, parent);
      gtk_tree_store_set (store, &pos, 0, enum_value->value_nick, 1, element, -1);

      if (child)
        {
          populate_store_aux (store, &pos, child);
          poppler_structure_element_iter_free (child);
        }
    }
  while (poppler_structure_element_iter_next (iter));
}


static const gchar *
enum_value_text (GType type, int value)
{
  GEnumClass *enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  GEnumValue *enum_value = g_enum_get_value (enum_class, value);
  return enum_value->value_nick;
}


static gchar *
format_doubles_array (gdouble *values, guint n_values)
{
  GString *buffer = g_string_new ("(");

  for (; n_values; n_values--, values++)
    g_string_append_printf (buffer, (n_values == 1) ? "%.2g" : "%.2g, ", *values);
  g_string_append_c (buffer, ')');

  return g_string_free (buffer, FALSE);
}


static GtkTreeStore *
populate_store (PopplerStructureElementIter *iter)
{
  GtkTreeStore *store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

  if (iter)
    {
      populate_store_aux (store, NULL, iter);
    }
  else
    {
      GtkTreeIter pos;

      gtk_tree_store_append (store, &pos, NULL);
      gtk_tree_store_set (store, &pos, 0, "<b>Not a Tagged-PDF</b>", 1, NULL, -1);
    }

  return store;
}


/*static void
pgd_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, PgdTaggedStructDemo *demo)
{*/
static void
pgd_selection_changed (GtkTreeSelection *selection, PgdTaggedStructDemo *demo)
{
  PopplerStructureBorderStyle border_style[4];
  GtkTreeModel *model;
  PopplerStructureElement *element;
  PopplerRectangle bbox;
  GtkTreeIter iter;
  gpointer elementptr;
  gchar *text;
  PopplerColor color[4];
  GdkPixbuf *pixbuf;
  gboolean is_table = FALSE;
  gdouble double_values[4];
  guint i;

  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 1, &elementptr, -1);
  element = POPPLER_STRUCTURE_ELEMENT (elementptr);

  /* Common attributes */
  text = g_strdup_printf ("%i", poppler_structure_element_get_page (element));
  gtk_label_set_text (GTK_LABEL (demo->page_num), text);
  g_free (text);

  gtk_label_set_text (GTK_LABEL (demo->id_value),
                      poppler_structure_element_get_id (element));
  gtk_label_set_text (GTK_LABEL (demo->title_value),
                      poppler_structure_element_get_title (element));
  gtk_label_set_text (GTK_LABEL (demo->lang_value),
                      poppler_structure_element_get_language (element));
  gtk_label_set_text (GTK_LABEL (demo->abbr_value),
                      poppler_structure_element_get_abbreviation (element));

  text = poppler_structure_element_get_alt_text (element);
  gtk_label_set_text (GTK_LABEL (demo->alt_text), (text == NULL) ? "" : text);
  g_free (text);

  text = poppler_structure_element_get_actual_text (element);
  gtk_label_set_text (GTK_LABEL (demo->actual_text), (text == NULL) ? "" : text);
  g_free (text);

  gtk_label_set_text (GTK_LABEL (demo->placement),
                      enum_value_text (POPPLER_TYPE_STRUCTURE_PLACEMENT,
                                       poppler_structure_element_get_placement (element)));
  gtk_label_set_text (GTK_LABEL (demo->writing_mode),
                      enum_value_text (POPPLER_TYPE_STRUCTURE_WRITING_MODE,
                                       poppler_structure_element_get_writing_mode (element)));

  if (poppler_structure_element_is_block (element) &&
      poppler_structure_element_get_bounding_box (element, &bbox))
    {
      text = g_strdup_printf ("(%.2g, %.2g, %.2g, %.2g)", bbox.x1, bbox.y1, bbox.x2, bbox.y2);
      gtk_label_set_text (GTK_LABEL (demo->bounding_box), text);
      g_free (text);
    }
  else
      gtk_label_set_text (GTK_LABEL (demo->bounding_box), "");

  /* Foreground color */
  if (poppler_structure_element_get_color (element, &color[0]))
    {
      pixbuf = pgd_pixbuf_new_for_color (&color[0]);
      gtk_image_set_from_pixbuf (GTK_IMAGE (demo->colors[0]), pixbuf);
      g_object_unref (pixbuf);
    }
  else
    gtk_image_set_from_pixbuf (GTK_IMAGE (demo->colors[0]), demo->checkerboard);

  /* Background color */
  if (poppler_structure_element_get_background_color (element, &color[0]))
    {
      pixbuf = pgd_pixbuf_new_for_color (&color[0]);
      gtk_image_set_from_pixbuf (GTK_IMAGE (demo->colors[1]), pixbuf);
      g_object_unref (pixbuf);
    }
  else
    gtk_image_set_from_pixbuf (GTK_IMAGE (demo->colors[1]), demo->checkerboard);

  /* Border color */
  if (poppler_structure_element_get_border_color (element, color))
    {
      for (i = 0; i < 4; i++)
        {
          pixbuf = pgd_pixbuf_new_for_color (&color[i]);
          gtk_image_set_from_pixbuf (GTK_IMAGE (demo->border_colors[i]), pixbuf);
          g_object_unref (pixbuf);
        }
    }
  else
    {
      for (i = 0; i < 4; i++)
        gtk_image_set_from_pixbuf (GTK_IMAGE (demo->border_colors[i]), demo->checkerboard);
    }

  /* Border thickness */
  if (poppler_structure_element_get_border_thickness (element, double_values))
    {
      text = g_strdup_printf ("(%.2g, %.2g, %.2g, %.2g)",
                              double_values[0], double_values[1],
                              double_values[2], double_values[3]);
      gtk_label_set_text (GTK_LABEL (demo->border_thickness), text);
      g_free (text);
    }
  else
    gtk_label_set_text (GTK_LABEL (demo->border_thickness), "");

  /* Border style / padding */
  if (poppler_structure_element_get_kind (element) == POPPLER_STRUCTURE_ELEMENT_TABLE)
    {
      poppler_structure_element_get_table_border_style (element, border_style);
      poppler_structure_element_get_table_padding (element, double_values);
      is_table = TRUE;
    }
  else
    {
      poppler_structure_element_get_border_style (element, border_style);
      poppler_structure_element_get_padding (element, double_values);
    }

  text = g_strdup_printf ("%s(%s, %s, %s, %s)", is_table ? "Table " : "",
                          enum_value_text (POPPLER_TYPE_STRUCTURE_BORDER_STYLE, border_style[0]),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_BORDER_STYLE, border_style[1]),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_BORDER_STYLE, border_style[2]),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_BORDER_STYLE, border_style[3]));
  gtk_label_set_text (GTK_LABEL (demo->border_style), text);
  g_free (text);

  text = g_strdup_printf ("%s(%.2g, %.2g, %.2g, %.2g)", is_table ? "Table " : "",
                          double_values[0], double_values[1], double_values[2], double_values[3]);
  gtk_label_set_text (GTK_LABEL (demo->padding), text);
  g_free (text);

  /* Block element attributes */
  if (poppler_structure_element_is_block (element))
    {
      gtk_label_set_text (GTK_LABEL (demo->text_align),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_TEXT_ALIGN,
                                           poppler_structure_element_get_text_align (element)));
      gtk_label_set_text (GTK_LABEL (demo->block_align),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_BLOCK_ALIGN,
                                           poppler_structure_element_get_block_align (element)));
      gtk_label_set_text (GTK_LABEL (demo->inline_align),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_INLINE_ALIGN,
                                           poppler_structure_element_get_inline_align (element)));

      text = g_strdup_printf ("%.2g ⨉ %.2g",
                              poppler_structure_element_get_width (element),
                              poppler_structure_element_get_height (element));
      gtk_label_set_text (GTK_LABEL (demo->width_height), text);
      g_free (text);

      text = g_strdup_printf ("%.2g / %.2g",
                              poppler_structure_element_get_space_before (element),
                              poppler_structure_element_get_space_after (element));
      gtk_label_set_text (GTK_LABEL (demo->space_before_after), text);
      g_free (text);

      text = g_strdup_printf ("%.2g / %.2g / %.2g",
                              poppler_structure_element_get_start_indent (element),
                              poppler_structure_element_get_end_indent (element),
                              poppler_structure_element_get_text_indent (element));
      gtk_label_set_text (GTK_LABEL (demo->indent_start_end_text), text);
      g_free (text);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (demo->text_align), "");
      gtk_label_set_text (GTK_LABEL (demo->block_align), "");
      gtk_label_set_text (GTK_LABEL (demo->inline_align), "");
      gtk_label_set_text (GTK_LABEL (demo->width_height), "");
      gtk_label_set_text (GTK_LABEL (demo->space_before_after), "");
      gtk_label_set_text (GTK_LABEL (demo->indent_start_end_text), "");
    }

  /* Inline element attributes */
  if (poppler_structure_element_is_inline (element))
    {
      if (poppler_structure_element_get_text_decoration_color (element, &color[0]))
        {
          pixbuf = pgd_pixbuf_new_for_color (&color[0]);
          gtk_image_set_from_pixbuf (GTK_IMAGE (demo->text_decoration_color), pixbuf);
          g_object_unref (pixbuf);
        }
      else
        gtk_image_set_from_pixbuf (GTK_IMAGE (demo->text_decoration_color), demo->checkerboard);

      gtk_label_set_text (GTK_LABEL (demo->text_decoration),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_TEXT_DECORATION,
                                           poppler_structure_element_get_text_decoration_type (element)));
      gtk_label_set_text (GTK_LABEL (demo->ruby_align),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_RUBY_ALIGN,
                                           poppler_structure_element_get_ruby_align (element)));
      gtk_label_set_text (GTK_LABEL (demo->ruby_position),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_RUBY_POSITION,
                                           poppler_structure_element_get_ruby_position (element)));
      gtk_label_set_text (GTK_LABEL (demo->glyph_orientation),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_GLYPH_ORIENTATION,
                                           poppler_structure_element_get_glyph_orientation (element)));

      text = g_strdup_printf ("%.2g", poppler_structure_element_get_text_decoration_thickness (element));
      gtk_label_set_text (GTK_LABEL (demo->text_decoration_thickness), text);
      g_free (text);

      text = g_strdup_printf ("%.2g / %.2g",
                              poppler_structure_element_get_line_height (element),
                              poppler_structure_element_get_baseline_shift (element));
      gtk_label_set_text (GTK_LABEL (demo->line_height_shift), text);
      g_free (text);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (demo->text_decoration), "");
      gtk_label_set_text (GTK_LABEL (demo->ruby_align), "");
      gtk_label_set_text (GTK_LABEL (demo->ruby_position), "");
      gtk_label_set_text (GTK_LABEL (demo->glyph_orientation), "");
      gtk_label_set_text (GTK_LABEL (demo->line_height_shift), "");
      gtk_label_set_text (GTK_LABEL (demo->text_decoration_thickness), "");
      gtk_image_set_from_pixbuf (GTK_IMAGE (demo->text_decoration_color), demo->checkerboard);
    }

  /* List-item element attributes */
  if (poppler_structure_element_get_kind (element) == POPPLER_STRUCTURE_ELEMENT_LIST_ITEM)
    {
      gtk_label_set_text (GTK_LABEL (demo->list_numbering),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_LIST_NUMBERING,
                                           poppler_structure_element_get_list_numbering (element)));
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (demo->list_numbering), "");
    }

  /* Form element attributes */
  if (poppler_structure_element_get_kind (element) == POPPLER_STRUCTURE_ELEMENT_FORM)
    {
      gtk_label_set_text (GTK_LABEL (demo->form_role),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_FORM_ROLE,
                                           poppler_structure_element_get_form_role (element)));
      gtk_label_set_text (GTK_LABEL (demo->form_state),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_FORM_STATE,
                                           poppler_structure_element_get_form_state (element)));

      text = poppler_structure_element_get_form_description (element);
      gtk_label_set_text (GTK_LABEL (demo->form_description), (text == NULL) ? "" : text);
      g_free (text);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (demo->form_role), "");
      gtk_label_set_text (GTK_LABEL (demo->form_state), "");
      gtk_label_set_text (GTK_LABEL (demo->form_description), "");
    }

  /* Table element attributes */
  if (poppler_structure_element_get_kind (element) == POPPLER_STRUCTURE_ELEMENT_TABLE)
    {
      gtk_label_set_text (GTK_LABEL (demo->table_scope),
                          enum_value_text (POPPLER_TYPE_STRUCTURE_TABLE_SCOPE,
                                           poppler_structure_element_get_table_scope (element)));

      text = poppler_structure_element_get_table_summary (element);
      gtk_label_set_text (GTK_LABEL (demo->form_description), (text == NULL) ? "" : text);
      g_free (text);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (demo->table_scope), "");
      gtk_label_set_text (GTK_LABEL (demo->form_description), "");
    }

  /* Grouping element attributes */
  gtk_label_set_text (GTK_LABEL (demo->column_widths), "");
  gtk_label_set_text (GTK_LABEL (demo->column_gaps), "");

  if (poppler_structure_element_is_grouping (element))
    {
      gdouble *values;
      guint n_values;

      text = g_strdup_printf ("%i", poppler_structure_element_get_column_count (element));
      gtk_label_set_text (GTK_LABEL (demo->column_count), text);
      g_free (text);

      if ((values = poppler_structure_element_get_column_widths (element, &n_values)) != NULL)
        {
          text = format_doubles_array (values, n_values);
          g_free (values);
          gtk_label_set_text (GTK_LABEL (demo->column_widths), text);
          g_free (text);
        }

      if ((values = poppler_structure_element_get_column_gaps (element, &n_values)) != NULL)
        {
          text = format_doubles_array (values, n_values);
          g_free (values);
          gtk_label_set_text (GTK_LABEL (demo->column_gaps), text);
          g_free (text);
        }
    }
  else
    gtk_label_set_text (GTK_LABEL (demo->column_count), "");

  gtk_text_buffer_set_text (demo->text_buffer, "", -1);

  if (poppler_structure_element_is_content (element))
    {
      const gchar *text = poppler_structure_element_get_text (element, FALSE);

      if (text)
        gtk_text_buffer_set_text (demo->text_buffer, text, -1);
      gtk_label_set_text (GTK_LABEL (demo->type_value), "Content");
    }
  else
    {
      if (poppler_structure_element_is_inline (element))
        gtk_label_set_text (GTK_LABEL (demo->type_value), "Inline");
      else if (poppler_structure_element_is_block (element))
        gtk_label_set_text (GTK_LABEL (demo->type_value), "Block");
      else if (poppler_structure_element_is_grouping (element))
        gtk_label_set_text (GTK_LABEL (demo->type_value), "Grouping");
      else
        gtk_label_set_text (GTK_LABEL (demo->type_value), "Structure");
    }
}


GtkWidget *
pgd_taggedstruct_create_widget (PopplerDocument *document)
{
  PopplerStructureElementIter *iter;
  PgdTaggedStructDemo *demo;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;
  GtkWidget *pane;
  GtkWidget *grid;
  GtkWidget *scroll;
  GtkWidget *w;
  GtkWidget *box;
  gint row;

  demo = g_new0 (PgdTaggedStructDemo, 1);

  demo->checkerboard = pgd_pixbuf_new_checkerboard ();

  iter = poppler_structure_element_iter_new (document);
  demo->store = populate_store (iter);
  poppler_structure_element_iter_free (iter);

  demo->view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (demo->store));

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (demo->view),
                                               0, "Type",
                                               renderer,
                                               "markup", 0,
                                               NULL);
  g_object_set (G_OBJECT (gtk_tree_view_get_column (GTK_TREE_VIEW (demo->view), 0)),
                "expand", TRUE, NULL);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (demo->view));
  gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (demo->view), TRUE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (demo->view), TRUE);
  gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (demo->view), FALSE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (demo->view), TRUE);

  pane = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), demo->view);
  gtk_paned_add1 (GTK_PANED (pane), scroll);

  row = 0;
  grid = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_grid_set_row_homogeneous (GTK_GRID (grid), FALSE);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 2);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Page:</b>", &demo->page_num, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Type:</b>", &demo->type_value, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>ID:</b>", &demo->id_value, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Title:</b>", &demo->title_value, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Language:</b>", &demo->lang_value, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Abbreviation:</b>", &demo->abbr_value, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Alt. Text:</b>", &demo->alt_text, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Actual Text:</b>", &demo->actual_text, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Placement:</b>", &demo->placement, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Writing Mode:</b>", &demo->writing_mode, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Bounding Box:</b>", &demo->bounding_box, NULL, &row);

  /* Used both for table-padding and other elements' padding */
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Padding:</b>", &demo->padding, NULL, &row);

  /* Put both foreground and background color in a single row */
  box = pgd_new_color_image_box (demo->colors, 2, demo->checkerboard);
  pgd_table_add_property_with_custom_widget (GTK_GRID (grid), "<b>FG/BG Color:</b>", box, &row);

  /* Used both for table-border-style and border-style */
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Border Style:</b>", &demo->border_style, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Border Thickness:</b>", &demo->border_thickness, NULL, &row);

  /* Border colors */
  box = pgd_new_color_image_box (demo->border_colors, 4, demo->checkerboard);
  pgd_table_add_property_with_custom_widget (GTK_GRID (grid), "<b>Border Colors:</b>", box, &row);

  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Width</b> ⨉ <b>Height</b>", &demo->width_height, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Text Align:</b>", &demo->text_align, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Block Align:</b>", &demo->block_align, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Inline Align:</b>", &demo->inline_align, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Space Before/After:</b>", &demo->space_before_after, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Indent Start/End/Text:</b>", &demo->indent_start_end_text, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Line Heigt/Shift:</b>", &demo->line_height_shift, NULL, &row);

  /* Text decoration color */
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Text Decoration:</b>", &demo->text_decoration, NULL, &row);
  box = pgd_new_color_image_box (&demo->text_decoration_color, 1, demo->checkerboard);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Text Decoration Thickness:</b>", &demo->text_decoration_thickness, NULL, &row);
  pgd_table_add_property_with_custom_widget (GTK_GRID (grid), "<b>Text Decoration Color:</b>", box, &row);

  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Ruby Align:</b>", &demo->ruby_align, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Ruby Position:</b>", &demo->ruby_position, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Glyph Orientation:</b>", &demo->glyph_orientation, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>List Numbering:</b>", &demo->list_numbering, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Form Role:</b>", &demo->form_role, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Form State:</b>", &demo->form_state, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Form Description:</b>", &demo->form_description, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Table Scope:</b>", &demo->table_scope, NULL, &row);

  /* Column count, widths and gaps */
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Column Count:</b>", &demo->column_count, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Column Widths:</b>", &demo->column_widths, NULL, &row);
  pgd_table_add_property_with_value_widget (GTK_GRID (grid), "<b>Column Gaps:</b>", &demo->column_gaps, NULL, &row);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), grid);
  gtk_paned_add2 (GTK_PANED (pane), scroll);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), (w = gtk_text_view_new ()));

  demo->text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (w));
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (w), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (w), FALSE);
  gtk_text_buffer_set_text (demo->text_buffer, "", -1);

  gtk_grid_attach (GTK_GRID (grid), scroll, 0, row, 2, 1);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (demo->view));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (pgd_selection_changed),
                    demo);

  g_object_weak_ref (G_OBJECT (pane),
                     (GWeakNotify) pgd_taggedstruct_free,
                     demo);

  gtk_paned_set_position (GTK_PANED (pane), 250);
  gtk_widget_show_all (pane);
  return pane;
}
