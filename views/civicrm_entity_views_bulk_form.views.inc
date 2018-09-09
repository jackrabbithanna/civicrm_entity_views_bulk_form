<?php

/**
 * @file
 * Views file for CiviCRM Entity Views Bulk Form
 */

/**
 * Implements hook_views_data_alter().
 *
 * @param $data
 *   Information about Views' tables and fields.
 */
function civicrm_entity_views_bulk_form_views_data_alter(&$data) {
  // Add the Title form field to
  $data['civicrm_contact']['civicrm_entity_views_bulk_form_form_element'] = array(
    'field' => array(
      'title' => t('CiviCRM Entity Form Element'),
      'help' => t('Provides configurable form to create entities in bulk.'),
      'handler' => 'civicrm_entity_views_bulk_form_field_handler',
    ),
  );
}
