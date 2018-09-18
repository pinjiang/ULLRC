#include "global.h"

gchar* get_string_from_json_object (JsonObject * object) {
  JsonNode *root;
  JsonGenerator *generator;
  gchar *text;

  /* Make it the root node */
  root = json_node_init_object (json_node_alloc (), object);
  generator = json_generator_new ();
  json_generator_set_root (generator, root);
  text = json_generator_to_data (generator, NULL);

  /* Release everything */
  g_object_unref (generator);
  json_node_free (root);
  return text;
}

//add by liyujia, 20180906

JsonObject* parse_json_object(const gchar* text) {
	JsonNode *root;
    JsonObject *object, *child;
    JsonParser *parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, text, -1, NULL)) {
		g_printerr ("Unknown message '%s', ignoring", text);
        g_object_unref (parser);
        goto out;
    }

    root = json_parser_get_root (parser);
    if (!JSON_NODE_HOLDS_OBJECT (root)) {
		g_printerr ("Unknown json message '%s', ignoring", text);
		g_object_unref (parser);
		goto out;
    }


    object = json_node_get_object (root);
    return object;          
		
out:
	g_free (text);
}
