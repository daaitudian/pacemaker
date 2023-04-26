/*
 * Copyright 2004-2023 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU Lesser General Public License
 * version 2.1 or later (LGPLv2.1+) WITHOUT ANY WARRANTY.
 */

#ifndef PCMK__CRM_COMMON_SCHEDULER__H
#  define PCMK__CRM_COMMON_SCHEDULER__H

#include <crm/common/actions.h>
#include <crm/common/nodes.h>
#include <crm/common/resources.h>
#include <crm/common/roles.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \file
 * \brief Scheduler API
 * \ingroup core
 */

//! Possible responses to loss of quorum
enum pe_quorum_policy {
    pcmk_no_quorum_freeze,  //<! Do not recover resources from outside partition
    pcmk_no_quorum_stop,    //<! Stop all resources in partition
    pcmk_no_quorum_ignore,  //<! Act as if partition still holds quorum
    pcmk_no_quorum_fence,   //<! Fence all nodes in partition
    pcmk_no_quorum_demote,  //<! Demote promotable resources and stop all others

#if !defined(PCMK_ALLOW_DEPRECATED) || (PCMK_ALLOW_DEPRECATED == 1)
    //! \deprecated Use pcmk_no_quorum_freeze instead
    no_quorum_freeze    = pcmk_no_quorum_freeze,

    //! \deprecated Use pcmk_no_quorum_stop instead
    no_quorum_stop      = pcmk_no_quorum_stop,

    //! \deprecated Use pcmk_no_quorum_ignore instead
    no_quorum_ignore    = pcmk_no_quorum_ignore,

    //! \deprecated Use pcmk_no_quorum_fence instead
    no_quorum_suicide   = pcmk_no_quorum_fence,

    //! \deprecated Use pcmk_no_quorum_demote instead
    no_quorum_demote    = pcmk_no_quorum_demote,
#endif
};

//! Scheduling options and conditions
enum pcmk_scheduler_flags {
    //! No scheduler flags set (compare with equality rather than bit set)
    pcmk_sched_none                     = 0ULL,

    // These flags are dynamically determined conditions

    //! Whether partition has quorum (via have-quorum property)
    pcmk_sched_quorate                  = (1ULL << 0),

    //! Whether cluster is symmetric (via symmetric-cluster property)
    pcmk_sched_symmetric_cluster        = (1ULL << 1),

    //! Whether cluster is in maintenance mode (via maintenance-mode property)
    pcmk_sched_in_maintenance           = (1ULL << 3),

    //! Whether fencing is enabled (via stonith-enabled property)
    pcmk_sched_fencing_enabled          = (1ULL << 4),

    //! Whether cluster has a fencing resource (via CIB resources)
    pcmk_sched_have_fencing             = (1ULL << 5),

    //! Whether any resource provides or requires unfencing (via CIB resources)
    pcmk_sched_enable_unfencing         = (1ULL << 6),

    //! Whether concurrent fencing is allowed (via concurrent-fencing property)
    pcmk_sched_concurrent_fencing       = (1ULL << 7),

    /*!
     * Whether resources removed from the configuration should be stopped (via
     * stop-orphan-resources property)
     */
    pcmk_sched_stop_removed_resources   = (1ULL << 8),

    /*!
     * Whether recurring actions removed from the configuration should be
     * cancelled (via stop-orphan-actions property)
     */
    pcmk_sched_cancel_removed_actions   = (1ULL << 9),

    //! Whether to stop all resources (via stop-all-resources property)
    pcmk_sched_stop_all                 = (1ULL << 10),

    /*!
     * Whether start failure should be treated as if migration-threshold is 1
     * (via start-failure-is-fatal property)
     */
    pcmk_sched_start_failure_fatal      = (1ULL << 12),

    //! \deprecated Do not use
    pcmk_sched_remove_after_stop        = (1ULL << 13),

    //! Whether unseen nodes should be fenced (via startup-fencing property)
    pcmk_sched_startup_fencing          = (1ULL << 14),

    /*!
     * Whether resources should be left stopped when their node shuts down
     * cleanly (via shutdown-lock property)
     */
    pcmk_sched_shutdown_lock            = (1ULL << 15),

    /*!
     * Whether resources' current state should be probed (when unknown) before
     * scheduling any other actions (via the enable-startup-probes property)
     */
    pcmk_sched_probe_resources          = (1ULL << 16),

    //! Whether the CIB status section has been parsed yet
    pcmk_sched_have_status              = (1ULL << 17),

    //! Whether the cluster includes any Pacemaker Remote nodes (via CIB)
    pcmk_sched_have_remote_nodes        = (1ULL << 18),

    // The remaining flags are scheduling options that must be set explicitly

    /*!
     * Whether to skip unpacking the CIB status section and stop the scheduling
     * sequence after applying node-specific location criteria (skipping
     * assignment, ordering, actions, etc.).
     */
    pcmk_sched_location_only            = (1ULL << 20),

    //! Whether sensitive resource attributes have been masked
    pcmk_sched_sanitized                = (1ULL << 21),

    //! Skip counting of total, disabled, and blocked resource instances
    pcmk_sched_no_counts                = (1ULL << 23),

    /*!
     * Skip deprecated code kept solely for backward API compatibility
     * (internal code should always set this)
     */
    pcmk_sched_no_compat                = (1ULL << 24),

    //! Whether node scores should be output instead of logged
    pcmk_sched_output_scores            = (1ULL << 25),

    //! Whether to show node and resource utilization (in log or output)
    pcmk_sched_show_utilization         = (1ULL << 26),

    /*!
     * Whether to stop the scheduling sequence after unpacking the CIB,
     * calculating cluster status, and applying node health (skipping
     * applying node-specific location criteria, assignment, etc.)
     */
    pcmk_sched_validate_only            = (1ULL << 27),
};

#ifdef __cplusplus
}
#endif

#endif // PCMK__CRM_COMMON_SCHEDULER__H
