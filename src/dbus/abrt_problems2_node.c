#include "libabrt.h"
#include "abrt_glib.h"
#include "problem_api.h"

#include "abrt_problems2_node.h"
#include "abrt_problems2_service.h"
#include <gio/gunixfdlist.h>

#define STRINGIZE(literal) #literal

struct _AbrtP2
{
    GObject parent_instance;
};

G_DEFINE_TYPE(AbrtP2, abrt_p2, G_TYPE_OBJECT)

static void abrt_p2_class_init(AbrtP2Class *klass)
{
}

static void abrt_p2_init(AbrtP2 *self)
{
}

AbrtP2*abrt_p2__new(char *dirname)
{
    return  g_object_new(TYPE_ABRT_P2, NULL);
}

static GList *abrt_g_variant_get_dict_keys(GVariant *dict)
{
    gchar *name = NULL;
    GVariant *value = NULL;
    GVariantIter iter;
    g_variant_iter_init(&iter, dict);

    GList *retval = NULL;
    /* No need to free 'name' and 'container' unless breaking out of the loop */
    while (g_variant_iter_loop(&iter, "{sv}", &name, &value))
        retval = g_list_prepend(retval, xstrdup(name));

    return retval;
}

static GVariant *handle_NewProblem(AbrtP2Service *service,
                                   GVariant *problem_info,
                                   gint32 flags,
                                   uid_t caller_uid,
                                   GUnixFDList *fd_list,
                                   GError **error)
{
    int r = abrt_p2_service_user_can_create_new_problem(service, caller_uid);
    if (r == 0)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_LIMITS_EXCEEDED,
                    "Too many problems have been recently created");
        return NULL;
    }
    if (r == -E2BIG)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_LIMITS_EXCEEDED,
                    "No more problems can be created");
        return NULL;
    }
    if (r < 0)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Failed to check NewProblem limits");
        return NULL;
    }

    char *problem_id = NULL;
    const char *new_path = NULL;

    GVariantDict pd;
    g_variant_dict_init(&pd, problem_info);

    /* Re-implement problem_data_add_basics(problem_info); - I don't want to
     * convert GVariant* to problem_data_t and back.
     *
     * The problem data should be converted to some kind of interface!
     */
    char *analyzer_str = NULL;
    GVariant *analyzer_element = g_variant_dict_lookup_value(&pd, FILENAME_ANALYZER, G_VARIANT_TYPE_STRING);
    if (analyzer_element == NULL)
    {
        analyzer_str = xstrdup("libreport");
        g_variant_dict_insert(&pd, FILENAME_ANALYZER, "s", analyzer_str);
    }
    else
    {
        analyzer_str = xstrdup(g_variant_get_string(analyzer_element, NULL));
        g_variant_unref(analyzer_element);
    }

    char *type_str = NULL;
    GVariant *type_element = g_variant_dict_lookup_value(&pd, FILENAME_TYPE, G_VARIANT_TYPE_STRING);
    if (type_element == NULL)
    {
         type_str = xstrdup(analyzer_str);
    }
    else
    {
         type_str = xstrdup(g_variant_get_string(type_element, NULL));
         g_variant_unref(type_element);
    }

    GVariant *uuid_element = g_variant_dict_lookup_value(&pd, FILENAME_UUID, G_VARIANT_TYPE_STRING);
    if (uuid_element == NULL)
    {
        GVariant *duphash_element = g_variant_dict_lookup_value(&pd, FILENAME_DUPHASH, G_VARIANT_TYPE_STRING);
        if (duphash_element != NULL)
        {
            g_variant_dict_insert_value(&pd, FILENAME_UUID, duphash_element);
            g_variant_unref(duphash_element);
        }
        else
        {
            /* start hash */
            sha1_ctx_t sha1ctx;
            sha1_begin(&sha1ctx);

            /*
             * To avoid spurious hash differences, sort keys so that elements are
             * always processed in the same order:
             */
            GList *list = abrt_g_variant_get_dict_keys(problem_info);
            list = g_list_sort(list, (GCompareFunc)strcmp);
            for (GList *l = list; l != NULL; l = g_list_next(l))
            {
                GVariant *element = g_variant_dict_lookup_value(&pd, (const char *)l->data, G_VARIANT_TYPE_STRING);
                /* do not hash items which are binary or file descriptor */
                if (element == NULL)
                    continue;

                gsize size = 0;
                const char *content = g_variant_get_string(element, &size);
                sha1_hash(&sha1ctx, content, size);
            }
            g_list_free_full(list, free);

            /* end hash */
            char hash_bytes[SHA1_RESULT_LEN];
            sha1_end(&sha1ctx, hash_bytes);
            char hash_str[SHA1_RESULT_LEN*2 + 1];
            bin2hex(hash_str, hash_bytes, SHA1_RESULT_LEN)[0] = '\0';

            g_variant_dict_insert(&pd, FILENAME_UUID, "s", hash_str);
        }
    }

    /* Sanitize UID
     */
    GVariant *uid_element =  g_variant_dict_lookup_value(&pd, FILENAME_UID, G_VARIANT_TYPE_STRING);
    char *uid_str = NULL;
    if (caller_uid != 0 || uid_element == NULL)
    {   /* set uid field to caller's uid if caller is not root or root doesn't pass own uid */
        log_info("Adding UID %lu to the problem info", (long unsigned)caller_uid);
        uid_str = xasprintf("%lu", (long unsigned)caller_uid);
        g_variant_dict_insert(&pd, FILENAME_UID, "s", uid_str);
    }
    else
        uid_str = xstrdup(g_variant_get_string(uid_element, NULL));

    if (uid_element != NULL)
        g_variant_unref(uid_element);

    GVariant *real_problem_info = g_variant_dict_end(&pd);

    new_path = abrt_p2_service_save_problem(service, type_str, real_problem_info, fd_list, caller_uid, &problem_id, error);

    g_variant_unref(real_problem_info);
    free(type_str);
    free(analyzer_str);

    if (problem_id)
        notify_new_path(problem_id);

    free(problem_id);

    if (new_path == NULL)
        return NULL;

    return g_variant_new("(o)", new_path);
}

static GVariant *handle_GetSession(AbrtP2Service *service, const char *caller, GError **error)
{
    const char *session_path = abrt_p2_service_session_path(service, caller, error);

    if (session_path == NULL)
        return NULL;

    return g_variant_new("(o)", session_path);
}

static GVariant *handle_GetProblems(AbrtP2Service *service, uid_t caller_uid)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));

    GList *problem_nodes = abrt_p2_service_get_problems_nodes(service, caller_uid);
    for (GList *p = problem_nodes; p != NULL; p = g_list_next(p))
        g_variant_builder_add(&builder, "o", (char*)p->data);
    g_list_free(problem_nodes);

    return g_variant_new("(ao)", &builder);
}


static GVariant *handle_DeleteProblems(AbrtP2Service *service,
            GVariant *entries, uid_t caller_uid, GError **error)
{
    GVariantIter *iter;
    gchar *entry_node;
    g_variant_get(entries, "ao", &iter);
    while (g_variant_iter_loop(iter, "o", &entry_node))
    {
        if (abrt_p2_service_remove_problem(service, entry_node, caller_uid, error) != 0)
        {
            g_free(entry_node);
            return NULL;
        }
    }

    return NULL;
}

/* D-Bus method handler
 */
static void dbus_method_call(GDBusConnection *connection,
                        const gchar *caller,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *method_name,
                        GVariant    *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer    user_data)
{
    log_debug("Problems2 method : %s", method_name);

    /* Check sanity */
    if (strcmp(interface_name, "org.freedesktop.Problems2") != 0)
    {
        error_msg("Unsupported interface %s", interface_name);
        return;
    }

    uid_t caller_uid;
    GVariant *response;

    GError *error = NULL;
    AbrtP2Service *service = abrt_p2_object_service(user_data);
    caller_uid = abrt_p2_service_caller_uid(service, caller, &error);
    if (caller_uid == (uid_t) -1)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        return;
    }

    if (strcmp("NewProblem", method_name) == 0)
    {
        GDBusMessage *msg = g_dbus_method_invocation_get_message(invocation);
        GUnixFDList *fd_list = g_dbus_message_get_unix_fd_list(msg);

        GVariant *data = g_variant_get_child_value(parameters, 0);
        gint32 flags;
        g_variant_get_child(parameters, 1, "i", &flags);

        response = handle_NewProblem(service, data, flags, caller_uid, fd_list, &error);
        g_variant_unref(data);
    }
    else if (strcmp("GetSession", method_name) == 0)
    {
        response = handle_GetSession(service, caller, &error);
    }
    else if (strcmp("GetProblems", method_name) == 0)
    {
        response = handle_GetProblems(service, caller_uid);
    }
    else if (strcmp("GetProblemData", method_name) == 0)
    {
        /* Parameter tuple is (0) */
        const char *entry_path;
        g_variant_get(parameters, "(&o)", &entry_path);

        response = abrt_p2_service_entry_problem_data(service, entry_path, caller_uid, &error);
    }
    else if (strcmp("DeleteProblems", method_name) == 0)
    {
        GVariant *array = g_variant_get_child_value(parameters, 0);
        response = handle_DeleteProblems(service, array, caller_uid, &error);
        g_variant_unref(array);
    }
    else
    {
        error_msg("BUG: org.freedesktop.Problems2 does not have method: %s", method_name);
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
                "The method has to be implemented");
        return;
    }

    if (error != NULL)
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    g_dbus_method_invocation_return_value(invocation, response);
}

GDBusInterfaceVTable *abrt_p2_vtable(void)
{
    static GDBusInterfaceVTable default_vtable =
    {
        .method_call = dbus_method_call,
        .get_property = NULL,
        .set_property = NULL,
    };

    return &default_vtable;
}
