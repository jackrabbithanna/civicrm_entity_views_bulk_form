<?php

/**
 * @file
 * A Views field handler for creating an CiviCRM Entities.
 */

/**
 * Class civicrm_entity_views_bulk_form_field_handler
 */
class civicrm_entity_views_bulk_form_field_handler  extends views_handler_field {

  private function get_entity_type_options() {
    $options = ['' => '- Select -'];
    // for now just support a few entity types..
    // could add alter hook here
    switch ($this->view->base_table) {
      case 'civicrm_contact':
        $options['civicrm_activity'] = 'Activity';
        $options['civicrm_participant'] = 'Participant';
        break;
    }
    return $options;
  }

  /**
   * Get subtype options, based on if custom fields have per type options for an entity
   *
   * @param $entity_type
   *
   * @return array
   */
  private function get_custom_field_subtypes($entity_type) {
    $options = ['' => '- Select -'];
    switch($entity_type) {
      case 'civicrm_activity':
        $options += _civicrm_entity_views_bulk_form_get_activity_type_options();
      break;
    }
    return $options;
  }

  /**
   * Builds an array of custom field metadata for a given activity type id
   *
   * @param $entity_type
   * @return array
   */
  private function get_field_metadata_for_entity_type($entity_type) {
    $entity_field_metadata = [];
    $entity_type_options = $this->get_entity_type_options();
    if (!empty($entity_type_options[$entity_type])) {
      $wrapper = entity_metadata_wrapper($entity_type);
      foreach ($wrapper as $name => $child) {
        // custom fields
        $api_field_metadata = $child->info();
        if (strpos($api_field_metadata['name'], 'field_') === 0) {
          continue;
        }
        if (strpos($api_field_metadata['name'], 'custom_') === 0) {
          $entity_field_metadata[$name] = $api_field_metadata;
          // here we can make sure to limit custom fields to subtypes of entity
        }
        // stock entity properties
        elseif (strpos($api_field_metadata['name'], 'custom_') === FALSE) {
          $entity_field_metadata[$name] = $api_field_metadata;
        }
      }
    }
    dsm($entity_field_metadata);
    return $entity_field_metadata;
  }

  private function get_field_fapi_from_metadata($field_metadata, $entity_type, $for_defaults = FALSE) {
    $name = $field_metadata['name'];
    if (isset($field_metadata['widget']['widget'])) {
      $field_fapi = array(
        '#type' => $field_metadata['widget']['widget'],
        '#title' => $field_metadata['label'],
        '#description' => !empty($field_metadata['description']) ? $field_metadata['description'] : '',
      );

      // set api required fields to be required on form
      /*
      if ($for_defaults && !empty($field_metadata['required'])) {
        $field_fapi['#required'] = TRUE;
      }
      */

      if ($field_fapi['#type'] == 'civi_fk_reference') {
        switch ($field_metadata['widget']['subtype']) {
          case 'autocomplete':
            $field_fapi['#type'] = 'textfield';
            $field_fapi['#autocomplete_path'] = 'civicrm-entity/autocomplete/' . $field_metadata['widget']['entity'];
            break;
          case 'select':
            $field_fapi['#type'] = 'select';
            $field_fapi['#options'] = !empty($entity_form[$name]['#required']) ? _civicrm_entity_get_entities_list($field_metadata['widget']['entity']) : array(NULL => 'None') + _civicrm_entity_get_entities_list($field_metadata['widget']['entity']);
            break;
          default:
            $field_fapi['#type'] = 'textfield';
            break;
        }
      }

      // set the #size and #maxlength FAPI properties if the #type is a textfield
      if ($field_fapi['#type'] == 'textfield') {
        if (!empty($field_metadata['size'])) {
          $field_fapi['#size'] = $field_metadata['size'];
        }
        if (!empty($field_metadata['maxlength'])) {
          $field_fapi['#maxlength'] = $field_metadata['maxlength'];
        }
        if (!$for_defaults) {
          $field_fapi['#attributes']['placeholder'] = 'Use Default';
        }
      }

      //set rows and cols if available for textarea or text_format
      if ($field_fapi['#type'] == 'textarea' || $field_fapi['#type'] == 'text_format') {
        $field_fapi['#type'] = 'textarea';
        // set rows value if not empty
        if (!empty($field_metadata['rows'])) {
          $field_fapi['#rows'] = $field_metadata['rows'];
        }
        if (!empty($field_metadata['cols'])) {
          $field_fapi['#cols'] = $field_metadata['cols'];
        }
      }

      //set the options for select options, radios, and checkboxes
      $types_with_options = ['select', 'checkboxes', 'radios'];
      if (in_array($field_fapi['#type'], $types_with_options)) {
        if (!empty($field_metadata['widget']['options'])) {
          $field_fapi['#options'] = $field_metadata['widget']['options'];
          if($for_defaults) {
            $field_fapi['#options']['no_default'] = '- No Default -';
          }
          else {
            $field_fapi['#options']['use_default'] = '- Use Default -';
          }
        }
      }
      // Date field handling, check for date_api module to use date select widget
      // else fallback to textfield
      elseif ($field_fapi['#type'] == 'date_select') {
        if (module_exists('date_api')) {
          if (module_exists('date_popup')) {
            $field_fapi['#type'] = 'date_popup';
          }
          $field_fapi['#format'] = $field_metadata['widget']['format'];
          $field_fapi['#date_year_range'] = '-115:+10';
        }
        else {
          $field_fapi['#type'] = 'textfield';
        }
      }

      // event special handling
      if ($entity_type == 'civicrm_event') {
         if ($for_defaults && $name == 'event_type_id') {
          $field_fapi['#required'] = TRUE;
        }
      }

      // make sure to disable id field, and any custom field with is_view = 1
      if ($name == 'id' || !empty($field_metadata['is_view'])) {
        $field_fapi['#disabled'] = TRUE;
      }
      return $field_fapi;
    }
    return [];
  }

  /**
   * @inheritdoc
   */
  function construct() {
    parent::construct();
    $this->additional_fields['id'] = 'id';
  }

  /**
   * @inheritdoc
   */
  function query() {
    $this->ensure_my_table();
    $this->add_additional_fields();
  }

  /**
   * @inheritdoc
   */
  function option_definition() {
    $options = parent::option_definition();
    $options['civicrm_entity_views_bulk_form_included_fields'] = ['default' => []];
    $options['civicrm_entity_views_bulk_form_entity_type'] = ['default' => ''];
    $options['civicrm_entity_views_bulk_form_custom_field_subtype'] = ['default', ''];
    return $options;
  }

  /**
   * Creates the form element options for the Views field settings form.
   *
   * @param $form
   * @param $form_state
   */
  function options_form(&$form, &$form_state)
  {
    parent::options_form($form, $form_state);
    $form_state['no_cache'] = FALSE;
    $options = [];
    if (!empty($form_state['values']['options']['civicrm_entity_views_bulk_form_entity_type'])) {
      $entity_field_options = $this->get_field_metadata_for_entity_type($form_state['values']['options']['civicrm_entity_views_bulk_form_entity_type']);
    }
    elseif (!empty($this->options['civicrm_entity_views_bulk_form_entity_type'])) {
      $entity_field_options = $this->get_field_metadata_for_entity_type($this->options['civicrm_entity_views_bulk_form_entity_type']);
    }
    if (!empty($entity_field_options)) {
      foreach ($entity_field_options as $field_id => $metadata) {
        $options[$field_id] = $metadata['label'] . (!empty($metadata['required']) ? '* (Required)' : '');
      }
    }

    $form['civicrm_entity_views_bulk_form_entity_type'] = [
      '#type' => 'select',
      '#required' => TRUE,
      '#title' => t('Entity Type'),
      '#description' => t('First choose an entity type that will be created. Available entity types depends on base table of the view.'),
      '#options' => $this->get_entity_type_options(),
      '#default_value' => $this->options['civicrm_entity_views_bulk_form_entity_type'],
      '#ajax' => [
        'callback' => 'civicrm_entity_views_bulk_form_entity_type_ajax_callback',
        'wrapper' => 'views-civicrm-entity-bulk-form-fields',
      ],
    ];

    $form['civicrm_entity_views_bulk_form_fields'] = [
      '#type' => 'fieldset',
      '#title' => 'Fields',
      '#collapsible' => FALSE,
      '#prefix' => '<div id="views-civicrm-entity-bulk-form-fields">',
      '#suffix' => '</div>',
    ];
    if (isset($form_state['values']['options']['civicrm_entity_views_bulk_form_entity_type'])) {
      $entity_type = $form_state['values']['options']['civicrm_entity_views_bulk_form_entity_type'];
    }
    elseif (!empty($this->options['civicrm_entity_views_bulk_form_entity_type'])) {
      $entity_type = $this->options['civicrm_entity_views_bulk_form_entity_type'];
    }
    if (!empty($entity_type)) {
      $custom_field_subtype_options = $this->get_custom_field_subtypes($entity_type);
      if (!empty($form_state['values']['civicrm_entity_views_bulk_form_fields']['custom_field_subtype']) && empty($custom_field_subtype_options[$form_state['values']['civicrm_entity_views_bulk_form_fields']['custom_field_subtype']])) {
        $entity_subtype = $form_state['values']['civicrm_entity_views_bulk_form_fields']['custom_field_subtype'];
      }
      elseif (!empty($this->options['civicrm_entity_views_bulk_form_custom_field_subtype']))  {
        $entity_subtype = $this->options['civicrm_entity_views_bulk_form_custom_field_subtype'];
      }
      if (!empty($entity_subtype) && empty($custom_field_subtype_options[$entity_subtype])) {
        $this->options['civicrm_entity_views_bulk_form_custom_field_subtype'] = '';
      }
      $form['civicrm_entity_views_bulk_form_fields']['custom_field_subtype'] = [
        '#type' => 'select',
        '#title' => 'Custom field subtype',
        '#description' => t('Check the boxes next to the fields to include'),
        '#options' => $custom_field_subtype_options,
        '#default_value' => $this->options['civicrm_entity_views_bulk_form_custom_field_subtype'],
      ];
      $form['civicrm_entity_views_bulk_form_fields']['help_text'] = [
        '#type'   => 'markup',
        '#markup' => 'Choose which fields should be added to the form.',
      ];
      $form['civicrm_entity_views_bulk_form_fields']['field_selections'] = [
        '#type' => 'checkboxes',
        '#description' => t('Check the boxes next to the fields to include'),
        '#options' => $options,
        '#default_value' => $this->options['civicrm_entity_views_bulk_form_included_fields'],
      ];
    }
  }

  /**
   * Options form submit handler
   *
   * @param $form
   * @param $form_state
   */
  function options_submit(&$form, &$form_state) {
    parent::options_submit($form, $form_state);
    $this->options['civicrm_entity_views_bulk_form_included_fields'] = $form_state['values']['options']['civicrm_entity_views_bulk_form_fields']['field_selections'];
    $this->options['civicrm_entity_views_bulk_form_custom_field_subtype'] = $form_state['values']['options']['civicrm_entity_views_bulk_form_fields']['custom_field_subtype'];
  }

  /**
   * If the view is using a table style, provide a
   * placeholder for a "select all" checkbox.
   *
   * @return string
   */
  function label() {
    if (!empty($this->view->style_plugin) && $this->view->style_plugin instanceof views_plugin_style_table) {
      return '<!--views-bulk-civicrm-entity-form-select-all-->';
    }
    else {
      return parent::label();
    }
  }

  /**
   * Render a Views form item placeholder.
   * This causes Views to wrap the View in a form.
   *
   * @param $values
   * @return string
   */
  function render($values) {
    return '<!--form-item-' . $this->options['id'] . '--' . $this->view->row_index . '-->';
  }

  /**
   * Add to and alter the form created by Views.
   *
   * @param $form
   * @param $form_state
   */
  function views_form(&$form, &$form_state) {
    $form['#attached']['css'][] = drupal_get_path('module', 'civicrm_entity_views_bulk_form') . '/css/civicrm_entity_views_bulk_form.css';
    $form['#attached']['js'][] = drupal_get_path('module', 'civicrm_entity_views_bulk_form') . '/js/civicrm_entity_views_bulk_form.js';
    $class = 'cevbf-views-form';

    $form['#prefix'] = '<div class="' . $class . '">';
    $form['#suffix'] = '</div>';

    // Force browser to reload the page if Back is hit.
    if (!empty($_SERVER['HTTP_USER_AGENT']) && preg_match('/msie/i', $_SERVER['HTTP_USER_AGENT'])) {
      drupal_add_http_header('Cache-Control', 'no-cache'); // works for IE6+
    }
    else {
      drupal_add_http_header('Cache-Control', 'no-store'); // works for Firefox and other browsers
    }

    // only render the form is the entity type has been configured in the Views field settings
    if (!empty($this->options['civicrm_entity_views_bulk_form_entity_type'])) {
      $entity_type = $this->options['civicrm_entity_views_bulk_form_entity_type'];
      // fetch field metadata for a particular entity type
      $form_state['field_metadata'] = $field_metadata = $this->get_field_metadata_for_entity_type($entity_type);
      // get selected custom fields from Views field configuration
      $selected_fields_settings = $this->options['civicrm_entity_views_bulk_form_included_fields'];
      // build an array of custom field keys that have been selected in the Views field configuration
      $selected_fields = [];
      foreach ($selected_fields_settings as $id => $value) {
        if (!empty($value)) {
          $selected_fields[] = $id;
        }
      }
      // save it to the form state for use in the submit handler
      $form_state['selected_fields_from_config'] = $selected_fields;

      // default value field set
      $form['default_options_section'] = [
        '#type' => 'fieldset',
        '#collapsible' => TRUE,
        '#collapsed' => FALSE,
        '#tree' => TRUE,
        '#weight' => -1,
        '#title' => t('Default Values'),
        '#description' => t('You can optionally choose default values for each configured field'),
      ];
      // build fapi for the fields selected in the View field config, for setting default values
      if (!empty($form_state['selected_fields_from_config'])) {
        foreach ($form_state['selected_fields_from_config'] as $field_id) {
          $field_fapi = $this->get_field_fapi_from_metadata($field_metadata[$field_id], $entity_type, TRUE);
          if (!empty($field_fapi)) {
            $form['default_options_section'][$field_id] = $field_fapi;
          }
        }
      }

      // Create a container for our replacements
      $form[$this->options['id']] = [
        '#type'   => 'container',
        '#tree'   => TRUE,
        '#weight' => 0,
      ];

      // Iterate over the result and add our replacement fields to the form.
      foreach ($this->view->result as $row_index => $row) {
        $form[$this->options['id']][$row_index] = [
          '#type' => 'container',
          '#tree' => TRUE,
          '#prefix' => '<table><tr>',
          '#suffix' => '</tr></table>'
        ];
        $form[$this->options['id']][$row_index]['entity_id'] = [
          '#type'  => 'value',
          '#value' => $row->id,
        ];
        $form[$this->options['id']][$row_index]['cevbf_select_row'] = [
          '#type' => 'checkbox',
          '#title' => t('Create Entity'),
          '#return_value' => 1,
          //'#theme_wrappers' => [],
          '#attributes'=> ['class' => ['views-bulk-civicrm-entity-form-select']],
          '#prefix' => '<td>',
          '#suffix' => '</td>',
        ];
        if (!empty($form_state['selected_fields_from_config'])) {
          foreach ($form_state['selected_fields_from_config'] as $field_id) {
            $field_fapi = $this->get_field_fapi_from_metadata($field_metadata[$field_id], $entity_type);
            if (!empty($field_fapi)) {
              $form[$this->options['id']][$row_index][$field_id] = $field_fapi;
              $form[$this->options['id']][$row_index][$field_id]['#prefix'] = '<td>';
              $form[$this->options['id']][$row_index][$field_id]['#suffix'] = '</td>';
            }
          }
        }
      }
      $form['#validate'][] = 'civicrm_entity_views_bulk_form_views_form_validate';
    }
    else {
      $form['error_message'] = [
        '#type' => 'markup',
        '#markup' => '<p>The Entity Type has not yet been configured in the "CiviCRM Entity Views Bulk Form Element" View field settings.</p>'
      ];
    }

  }

  /**
   * Form submit method.
   *
   * @param $form
   * @param $form_state
   */
  function views_form_submit($form, &$form_state)  {
    if (!empty($this->options['civicrm_entity_views_bulk_form_entity_type'])) {
      $entity_type = $this->options['civicrm_entity_views_bulk_form_entity_type'];
      // setup the default values array from submitted values from the Default Values fieldset
      $default_values = [];
      foreach ($form_state['selected_fields_from_config'] as $field_id) {
        if (is_array($form_state['values']['default_options_section'][$field_id])) {
          // make sure there are even any user selections for this multi option field
          if (count($form_state['values']['default_options_section'][$field_id])) {
            $default_options_return = [];
            // make sure the selected option id is set to default values array
            foreach ($form_state['values']['default_options_section'][$field_id] as $option_id => $value) {
              // if the checkbox is checked or item selected then option id value to default values array for the field
              if($form_state['field_metadata'][$field_id]['widget']['widget'] == 'checkboxes') {
                if ($value) {
                  $default_options_return[] = $option_id;
                }
              }
              else {
                if ($value || ($option_id == 0 && $value == 0)) {
                  $default_options_return[] = $option_id;
                }
              }
            }
            // if any were selected, then add to the default values array for the custom field
            if (count($default_options_return)) {
              $default_values[$field_id] = $default_options_return;
            }
          }
        }
        else {
          if(($form_state['field_metadata'][$field_id]['widget']['widget'] == 'radios' || $form_state['field_metadata'][$field_id]['widget']['widget'] == 'select') && isset($form_state['values']['default_options_section'][$field_id]) && $form_state['values']['default_options_section'][$field_id] != 'no_default') {
            $default_values[$field_id] = $form_state['values']['default_options_section'][$field_id];
          }
          elseif ($form_state['field_metadata'][$field_id]['widget']['widget'] == 'date_select' && isset($form_state['values']['default_options_section'][$field_id]) && is_array($form_state['values']['default_options_section'][$field_id])) {
            $default_values[$field_id] = !empty($form_state['values']['default_options_section'][$field_id]['date']) ? $form_state['values']['default_options_section'][$field_id]['date'] : '';
            $default_values[$field_id] .= !empty($form_state['values']['default_options_section'][$field_id]['time']) ? (' ' . date('H:i:s', strtotime($form_state['values']['default_options_section'][$field_id]['time']))) : '';
            if(empty($default_values[$field_id])) {
              unset($default_values[$field_id]);
            }
          }
          elseif (!empty($form_state['values']['default_options_section'][$field_id]) && $form_state['values']['default_options_section'][$field_id] != 'no_default') {
            $default_values[$field_id] = $form_state['values']['default_options_section'][$field_id];
          }

        }
      }

      // Iterate over the view results.
      foreach ($this->view->result as $row_index => $row) {
        // if this row is checked to create the activity, build the CiviCRM API call parameters and make the call
        if (!empty($form_state['values'][$this->options['id']][$row_index]['cevbf_select_row'])) {
          // basic parameters for every activity
          $entity = new CiviCRMEntity(array(), $entity_type);
          $entity->is_new = TRUE;
          $entity->type = $entity_type;

          // for each configured field in the Views field settings, process the input into parameter CiviCRM expects...
          foreach ($form_state['selected_fields_from_config'] as $field_id) {
            if (isset($form_state['values'][$this->options['id']][$row_index][$field_id])) {
              // if the form element value is an array, then we have a multi select or multiple checkboxes field...needs special handling
              if (is_array($form_state['values'][$this->options['id']][$row_index][$field_id])) {
                // make sure there are even any user selections for this multi option field
                if (count($form_state['values'][$this->options['id']][$row_index][$field_id])) {
                  $options_return = [];
                  // make sure the selected option id is passed to the API parameters
                  foreach ($form_state['values'][$this->options['id']][$row_index][$field_id] as $option_id => $value) {
                    // if the checkbox is checked or item selected then add to the API parameters
                    if($form_state['field_metadata'][$field_id]['widget']['widget'] == 'checkboxes') {
                      if ($value) {
                        $options_return[] = $option_id;
                      }
                    }
                    else {
                      if ($value || ($option_id == 0 && $value == 0)) {
                        $options_return[] = $option_id;
                      }
                    }
                  }
                  // if any were selected, then actually add to the parameter for the custom field
                  if (count($options_return)) {
                    $entity->{$field_id} = $options_return;
                  }
                }
              }
              // form element only has one value
              else {
                // if there is no data entered for a Money field, it was creating a 0 value anyway on the Activity...this if clause prevents it....if you wanted a no data entry to cause a 0 value, then remove....
                if ($form_state['field_metadata'][$field_id]['type'] == 'decimal' && strlen($form_state['values'][$this->options['id']][$row_index][$field_id]) == 0) {
                  if (!empty($default_values[$field_id])) {
                    $entity->{$field_id} = $default_values[$field_id];
                    continue;
                  }
                  else {
                    // skip if its a money field, and no value entered, and no default value provided
                    continue;
                  }
                }
                // add submitted value to API parameters
                $entity->{$field_id} = $form_state['values'][$this->options['id']][$row_index][$field_id];
              }
            }
            // populate from default values when appropriate....
            if (isset($default_values[$field_id])) {
              switch ($form_state['field_metadata'][$field_id]['widget']['widget']) {
                case 'textfield':
                case 'textarea':
                case 'date_select':
                case 'checkbox':
                  if(empty($form_state['values'][$this->options['id']][$row_index][$field_id])) {
                    $entity->{$field_id} = $default_values[$field_id];
                  }
                  break;
                case 'select':
                case 'radios':
                  if(!empty($params[$field_id]) && $params[$field_id] == 'use_default') {
                    $entity->{$field_id} = $default_values[$field_id];
                  }
                  break;
                case 'checkboxes':
                  if(!empty($params[$field_id])) {
                    foreach ($params[$field_id] as $index => $value) {
                      if ($value == 'use_default') {
                        $entity->{$field_id} = $default_values[$field_id];
                        break;
                      }
                    }
                  }
                  break;
                case 'Multi-Select':
                  if(!empty($params[$field_id])) {
                    foreach ($params[$field_id] as $index => $value) {
                      if (strpos($value, 'use_default') !== FALSE) {
                        $entity->{$field_id} = $default_values[$field_id];
                        break;
                      }
                    }
                  }
                  break;

              }
            }
            // if there are no default values, and a "use default" option is selected, unset because its giving conflicting commands
            else {
              switch ($form_state['field_metadata'][$field_id]['widget']['widget']) {
                case 'select':
                case 'radios':
                  if(!empty($params[$field_id]) && $params[$field_id] == 'use_default') {
                    unset($entity->{$field_id});
                  }
                  break;
                case 'checkboxes':
                  if (!empty($params[$field_id])) {
                    foreach ($params[$field_id] as $index => $value) {
                      if ($value == 'use_default') {
                        unset($entity->{$field_id});
                        break;
                      }
                    }
                  }
                  break;
                case 'Multi-Select':
                  if (!empty($params[$field_id])) {
                    foreach ($params[$field_id] as $index => $value) {
                      if (strpos($value, 'use_default') !== FALSE) {
                        unset($entity->{$field_id});
                        break;
                      }
                    }
                  }
                  break;
              }
            }
          }
          // create the activity with the CiviCRM API
          try {
            $wrapper = entity_metadata_wrapper($entity_type, $entity);
            $wrapper->save();
            drupal_set_message(check_plain($entity_type . ' created for contact with contact id:' . $form_state['values'][$this->options['id']][$row_index]['entity_id']));
          }
          catch (Exception $e) {
            drupal_set_message($e->getMessage());
            drupal_set_message(check_plain('Error creating ' . $entity_type . ' for contact with contact id:' . $form_state['values'][$this->options['id']][$row_index]['entity_id']));
          }
        }
      }
    }
    else {
      drupal_set_message('Entity Type not configured on CiviCRM Entity Views Bulk Form Views field configuration.');
    }
  }
}

/**
 * Validation handler for the Views Bulk CiviCRM Activity Views field handler
 *
 * Handle removing form validation errors for those rows which are not selected to have activities created
 *
 * @param $form
 * @param $form_state
 */
function civicrm_entity_views_bulk_form_views_form_validate(&$form, &$form_state) {
  $errors = form_get_errors();
  if (!empty($errors)) {
    // Clear errors.
    form_clear_error();
    // Clear error messages.
    $error_messages = drupal_get_messages('error');
    // Initialize an array where removed error messages are stored.
    $removed_messages = [];

    // filter out any validation errors on rows that are not selected to create an activity
    foreach ($errors as $element_key => $error_message) {
      $key_components = explode('[', $element_key);
      if($key_components[0] = 'civicrm_entity_views_bulk_form_form_element]') {
        $row_id = $key_components[1][0];
        $field_id = $key_components[2];

        // if the "create activity" checkbox is not checked, then no need to throw a validation error
        // OR if there is a required field, and there is a value in "default values" section for field, then skip the error too,
        if(empty($form_state['values']['civicrm_entity_views_bulk_form_form_element'][$row_id]['cevbf_select_row']) || !empty($form_state['values']['default_options_section'][$field_id])) {
          $removed_messages[] = $error_message;
          unset($errors[$element_key]);
        }
      }
    }

    // Reinstate remaining errors.
    if(!empty($errors)) {
      foreach ($errors as $name => $error) {
        form_set_error($name, $error);
        // form_set_error() calls drupal_set_message(), so we have to filter out
        // these from the error messages as well.
        $removed_messages[] = $error;
      }
    }

    // Reinstate remaining error messages (which, at this point, are messages that
    // were originated outside of the validation process).
    if(!empty($removed_messages)) {
      $messages_to_show = array_diff($error_messages['error'], $removed_messages);
      if (!empty($messages_to_show)) {
        foreach ($messages_to_show as $message) {
          drupal_set_message($message, 'error');
        }
      }
    }
  }
}

/**
 * Ajax callback for the Views field settings form, Activity Type select list
 *
 * @param $form
 * @param $form_state
 *
 * @return mixed
 */
function civicrm_entity_views_bulk_form_entity_type_ajax_callback(&$form, &$form_state) {
  return $form['options']['civicrm_entity_views_bulk_form_fields'];
}
