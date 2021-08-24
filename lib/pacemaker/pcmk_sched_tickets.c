/*
 * Copyright 2004-2021 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU General Public License version 2
 * or later (GPLv2+) WITHOUT ANY WARRANTY.
 */

#include <crm_internal.h>

#include <stdbool.h>
#include <glib.h>

#include <crm/crm.h>
#include <crm/pengine/status.h>
#include <pacemaker-internal.h>

#include "libpacemaker_private.h"

#define EXPAND_CONSTRAINT_IDREF(__set, __rsc, __name) do {                      \
        __rsc = pcmk__find_constraint_resource(data_set->resources, __name);    \
        if (__rsc == NULL) {                                                    \
            pcmk__config_err("%s: No resource found for %s", __set, __name);    \
            return FALSE;                                                       \
        }                                                                       \
    } while (0)

static void
rsc_ticket_new(const char *id, pe_resource_t *rsc_lh, pe_ticket_t *ticket,
               const char *state_lh, const char *loss_policy,
               pe_working_set_t *data_set)
{
    rsc_ticket_t *new_rsc_ticket = NULL;

    if (rsc_lh == NULL) {
        pcmk__config_err("Ignoring ticket '%s' because resource "
                         "does not exist", id);
        return;
    }

    new_rsc_ticket = calloc(1, sizeof(rsc_ticket_t));
    if (new_rsc_ticket == NULL) {
        return;
    }

    if (pcmk__str_eq(state_lh, RSC_ROLE_STARTED_S,
                     pcmk__str_null_matches|pcmk__str_casei)) {
        state_lh = RSC_ROLE_UNKNOWN_S;
    }

    new_rsc_ticket->id = id;
    new_rsc_ticket->ticket = ticket;
    new_rsc_ticket->rsc_lh = rsc_lh;
    new_rsc_ticket->role_lh = text2role(state_lh);

    if (pcmk__str_eq(loss_policy, "fence", pcmk__str_casei)) {
        if (pcmk_is_set(data_set->flags, pe_flag_stonith_enabled)) {
            new_rsc_ticket->loss_policy = loss_ticket_fence;
        } else {
            pcmk__config_err("Resetting '" XML_TICKET_ATTR_LOSS_POLICY
                             "' for ticket '%s' to 'stop' "
                             "because fencing is not configured", ticket->id);
            loss_policy = "stop";
        }
    }

    if (new_rsc_ticket->loss_policy == loss_ticket_fence) {
        crm_debug("On loss of ticket '%s': Fence the nodes running %s (%s)",
                  new_rsc_ticket->ticket->id, new_rsc_ticket->rsc_lh->id,
                  role2text(new_rsc_ticket->role_lh));

    } else if (pcmk__str_eq(loss_policy, "freeze", pcmk__str_casei)) {
        crm_debug("On loss of ticket '%s': Freeze %s (%s)",
                  new_rsc_ticket->ticket->id, new_rsc_ticket->rsc_lh->id,
                  role2text(new_rsc_ticket->role_lh));
        new_rsc_ticket->loss_policy = loss_ticket_freeze;

    } else if (pcmk__str_eq(loss_policy, "demote", pcmk__str_casei)) {
        crm_debug("On loss of ticket '%s': Demote %s (%s)",
                  new_rsc_ticket->ticket->id, new_rsc_ticket->rsc_lh->id,
                  role2text(new_rsc_ticket->role_lh));
        new_rsc_ticket->loss_policy = loss_ticket_demote;

    } else if (pcmk__str_eq(loss_policy, "stop", pcmk__str_casei)) {
        crm_debug("On loss of ticket '%s': Stop %s (%s)",
                  new_rsc_ticket->ticket->id, new_rsc_ticket->rsc_lh->id,
                  role2text(new_rsc_ticket->role_lh));
        new_rsc_ticket->loss_policy = loss_ticket_stop;

    } else {
        if (new_rsc_ticket->role_lh == RSC_ROLE_PROMOTED) {
            crm_debug("On loss of ticket '%s': Default to demote %s (%s)",
                      new_rsc_ticket->ticket->id, new_rsc_ticket->rsc_lh->id,
                      role2text(new_rsc_ticket->role_lh));
            new_rsc_ticket->loss_policy = loss_ticket_demote;

        } else {
            crm_debug("On loss of ticket '%s': Default to stop %s (%s)",
                      new_rsc_ticket->ticket->id, new_rsc_ticket->rsc_lh->id,
                      role2text(new_rsc_ticket->role_lh));
            new_rsc_ticket->loss_policy = loss_ticket_stop;
        }
    }

    pe_rsc_trace(rsc_lh, "%s (%s) ==> %s",
                 rsc_lh->id, role2text(new_rsc_ticket->role_lh), ticket->id);

    rsc_lh->rsc_tickets = g_list_append(rsc_lh->rsc_tickets, new_rsc_ticket);

    data_set->ticket_constraints = g_list_append(data_set->ticket_constraints,
                                                 new_rsc_ticket);

    if (!(new_rsc_ticket->ticket->granted) || new_rsc_ticket->ticket->standby) {
        rsc_ticket_constraint(rsc_lh, new_rsc_ticket, data_set);
    }
}

static gboolean
unpack_rsc_ticket_set(xmlNode *set, pe_ticket_t *ticket,
                      const char *loss_policy, pe_working_set_t *data_set)
{
    xmlNode *xml_rsc = NULL;
    pe_resource_t *resource = NULL;
    const char *set_id = NULL;
    const char *role = NULL;

    CRM_CHECK(set != NULL, return FALSE);
    CRM_CHECK(ticket != NULL, return FALSE);

    set_id = ID(set);
    if (set_id == NULL) {
        pcmk__config_err("Ignoring <" XML_CONS_TAG_RSC_SET "> without "
                         XML_ATTR_ID);
        return FALSE;
    }

    role = crm_element_value(set, "role");

    for (xml_rsc = first_named_child(set, XML_TAG_RESOURCE_REF);
         xml_rsc != NULL; xml_rsc = crm_next_same_xml(xml_rsc)) {

        EXPAND_CONSTRAINT_IDREF(set_id, resource, ID(xml_rsc));
        pe_rsc_trace(resource, "Resource '%s' depends on ticket '%s'",
                     resource->id, ticket->id);
        rsc_ticket_new(set_id, resource, ticket, role, loss_policy, data_set);
    }

    return TRUE;
}

static void
unpack_simple_rsc_ticket(xmlNode *xml_obj, pe_working_set_t *data_set)
{
    const char *id = NULL;
    const char *ticket_str = crm_element_value(xml_obj, XML_TICKET_ATTR_TICKET);
    const char *loss_policy = crm_element_value(xml_obj,
                                                XML_TICKET_ATTR_LOSS_POLICY);

    pe_ticket_t *ticket = NULL;

    const char *id_lh = crm_element_value(xml_obj, XML_COLOC_ATTR_SOURCE);
    const char *state_lh = crm_element_value(xml_obj,
                                             XML_COLOC_ATTR_SOURCE_ROLE);

    // experimental syntax from pacemaker-next (unlikely to be adopted as-is)
    const char *instance_lh = crm_element_value(xml_obj, XML_COLOC_ATTR_SOURCE_INSTANCE);

    pe_resource_t *rsc_lh = NULL;

    CRM_CHECK(xml_obj != NULL, return);

    id = ID(xml_obj);
    if (id == NULL) {
        pcmk__config_err("Ignoring <%s> constraint without " XML_ATTR_ID,
                         crm_element_name(xml_obj));
        return;
    }

    if (ticket_str == NULL) {
        pcmk__config_err("Ignoring constraint '%s' without ticket specified",
                         id);
        return;
    } else {
        ticket = g_hash_table_lookup(data_set->tickets, ticket_str);
    }

    if (ticket == NULL) {
        pcmk__config_err("Ignoring constraint '%s' because ticket '%s' "
                         "does not exist", id, ticket_str);
        return;
    }

    if (id_lh == NULL) {
        pcmk__config_err("Ignoring constraint '%s' without resource", id);
        return;
    } else {
        rsc_lh = pcmk__find_constraint_resource(data_set->resources, id_lh);
    }

    if (rsc_lh == NULL) {
        pcmk__config_err("Ignoring constraint '%s' because resource '%s' "
                         "does not exist", id, id_lh);
        return;

    } else if ((instance_lh != NULL) && !pe_rsc_is_clone(rsc_lh)) {
        pcmk__config_err("Ignoring constraint '%s' because resource '%s' "
                         "is not a clone but instance '%s' was requested",
                         id, id_lh, instance_lh);
        return;
    }

    if (instance_lh != NULL) {
        rsc_lh = find_clone_instance(rsc_lh, instance_lh, data_set);
        if (rsc_lh == NULL) {
            pcmk__config_warn("Ignoring constraint '%s' because resource '%s' "
                              "does not have an instance '%s'",
                              "'%s'", id, id_lh, instance_lh);
            return;
        }
    }

    rsc_ticket_new(id, rsc_lh, ticket, state_lh, loss_policy, data_set);
}

static gboolean
unpack_rsc_ticket_tags(xmlNode *xml_obj, xmlNode **expanded_xml,
                       pe_working_set_t *data_set)
{
    const char *id = NULL;
    const char *id_lh = NULL;
    const char *state_lh = NULL;

    pe_resource_t *rsc_lh = NULL;
    pe_tag_t *tag_lh = NULL;

    xmlNode *rsc_set_lh = NULL;

    *expanded_xml = NULL;

    CRM_CHECK(xml_obj != NULL, return FALSE);

    id = ID(xml_obj);
    if (id == NULL) {
        pcmk__config_err("Ignoring <%s> constraint without " XML_ATTR_ID,
                         crm_element_name(xml_obj));
        return FALSE;
    }

    // Check whether there are any resource sets with template or tag references
    *expanded_xml = pcmk__expand_tags_in_sets(xml_obj, data_set);
    if (*expanded_xml != NULL) {
        crm_log_xml_trace(*expanded_xml, "Expanded rsc_ticket");
        return TRUE;
    }

    id_lh = crm_element_value(xml_obj, XML_COLOC_ATTR_SOURCE);
    if (id_lh == NULL) {
        return TRUE;
    }

    if (!pcmk__valid_resource_or_tag(data_set, id_lh, &rsc_lh, &tag_lh)) {
        pcmk__config_err("Ignoring constraint '%s' because '%s' is not a "
                         "valid resource or tag", id, id_lh);
        return FALSE;

    } else if (rsc_lh) {
        // No template or tag is referenced
        return TRUE;
    }

    state_lh = crm_element_value(xml_obj, XML_COLOC_ATTR_SOURCE_ROLE);

    *expanded_xml = copy_xml(xml_obj);

    // Convert template/tag reference in "rsc" into resource_set under rsc_ticket
    if (!pcmk__tag_to_set(*expanded_xml, &rsc_set_lh, XML_COLOC_ATTR_SOURCE,
                          FALSE, data_set)) {
        free_xml(*expanded_xml);
        *expanded_xml = NULL;
        return FALSE;
    }

    if (rsc_set_lh != NULL) {
        if (state_lh != NULL) {
            // Move "rsc-role" into converted resource_set as a "role" attribute
            crm_xml_add(rsc_set_lh, "role", state_lh);
            xml_remove_prop(*expanded_xml, XML_COLOC_ATTR_SOURCE_ROLE);
        }

    } else {
        free_xml(*expanded_xml);
        *expanded_xml = NULL;
    }

    return TRUE;
}

void
pcmk__unpack_rsc_ticket(xmlNode *xml_obj, pe_working_set_t *data_set)
{
    xmlNode *set = NULL;
    gboolean any_sets = FALSE;

    const char *id = NULL;
    const char *ticket_str = crm_element_value(xml_obj, XML_TICKET_ATTR_TICKET);
    const char *loss_policy = crm_element_value(xml_obj, XML_TICKET_ATTR_LOSS_POLICY);

    pe_ticket_t *ticket = NULL;

    xmlNode *orig_xml = NULL;
    xmlNode *expanded_xml = NULL;

    gboolean rc = TRUE;

    CRM_CHECK(xml_obj != NULL, return);

    id = ID(xml_obj);
    if (id == NULL) {
        pcmk__config_err("Ignoring <%s> constraint without " XML_ATTR_ID,
                         crm_element_name(xml_obj));
        return;
    }

    if (data_set->tickets == NULL) {
        data_set->tickets = pcmk__strkey_table(free, destroy_ticket);
    }

    if (ticket_str == NULL) {
        pcmk__config_err("Ignoring constraint '%s' without ticket", id);
        return;
    } else {
        ticket = g_hash_table_lookup(data_set->tickets, ticket_str);
    }

    if (ticket == NULL) {
        ticket = ticket_new(ticket_str, data_set);
        if (ticket == NULL) {
            return;
        }
    }

    rc = unpack_rsc_ticket_tags(xml_obj, &expanded_xml, data_set);
    if (expanded_xml != NULL) {
        orig_xml = xml_obj;
        xml_obj = expanded_xml;

    } else if (!rc) {
        return;
    }

    for (set = first_named_child(xml_obj, XML_CONS_TAG_RSC_SET); set != NULL;
         set = crm_next_same_xml(set)) {

        any_sets = TRUE;
        set = expand_idref(set, data_set->input);
        if ((set == NULL) // Configuration error, message already logged
            || !unpack_rsc_ticket_set(set, ticket, loss_policy, data_set)) {
            if (expanded_xml != NULL) {
                free_xml(expanded_xml);
            }
            return;
        }
    }

    if (expanded_xml) {
        free_xml(expanded_xml);
        xml_obj = orig_xml;
    }

    if (!any_sets) {
        return unpack_simple_rsc_ticket(xml_obj, data_set);
    }
}
