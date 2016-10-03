<?php

/**
 * Creates entity structure from feed items.
 */
class FeedsEntityStructureProcessor extends FeedsProcessor {
  /**
   * Creates the base fields.
   *
   * @param array $field_data
   *   An associative array that contains the data for this field.
   * @param array $headers
   *   An associative array where the keys are the header names and the values are
   *   the header indices.
   *
   * @return array|bool|mixed|void
   *   Returns either the result of field_info_field, the newly created field, or
   *   FALSE if there were errors.
   */
  protected function initialize_field($field_data, $headers) {
    $final_field_data = array();
    foreach (array_keys($headers) as $field_property_name) {
      drupal_alter('csv2fields_field_base_' . $field_property_name, $final_field_data, $field_data);
    }
    drupal_alter('csv2fields_field_base', $final_field_data, $field_data);

    // If neither a field_name or type are set, this is probably not a legitimate
    // attempt at importing a field - more likely an empty line.
    if (!$final_field_data['field_name'] && !$final_field_data['type']) {
      return FALSE;
    }

    if (!$final_field_data['field_name']) {
      $final_field_data['errors'][] = dt('Missing field_name');
    }

    if (!$final_field_data['type']) {
      $final_field_data['errors'][] = dt('Missing type');
    }

    if (isset($final_field_data['errors']) && count($final_field_data['errors'])) {
      foreach ($final_field_data['errors'] as $error) {
        drush_log(dt('@error on row @row_number', array(
          '@row_number' => $field_data['row_number'],
          '@error' => $error,
        )), 'error');
      }
      return FALSE;
    }
    else {
      $field = field_info_field($final_field_data['field_name']);
      if ($field) {
        return $field;
      }
      return field_create_field($final_field_data);
    }
  }

  /**
   * Attaches a field to an instance, with settings, widgets, etc ready to go.
   *
   * @param array $field_data
   *   An associative array that contains the data for this field.
   * @param string $entity_type
   *   The entity type to attach to.
   * @param string $bundle
   *   The bundle type to attach to.
   * @param string $field_name
   *   The name of the field.
   * @param array $headers
   *   An associative array where the keys are the header names and the values are
   *   the header indices.
   *
   * @return bool|mixed
   *   Either returns the new entity instance or FALSE if there were errors.
   */
  protected function initialize_instance(&$field_data, $entity_type, $bundle, $field_name, $headers) {
    $final_field_data = array(
      'entity_type' => $entity_type,
      'bundle' => $bundle,
      'field_name' => $field_name,
    );
    foreach (array_keys($headers) as $field_property_name) {
      drupal_alter('csv2fields_field_instance_' . $field_property_name, $final_field_data, $field_data);
    }
    drupal_alter('csv2fields_field_instance', $final_field_data, $field_data);
    if (isset($final_field_data['errors']) && count($final_field_data['errors'])) {
      foreach ($final_field_data['errors'] as $error) {
        drush_log(dt('@error on row @row_number', array(
          '@row_number' => $field_data['row_number'],
          '@error' => $error,
        )), 'error');
      }
      return FALSE;
    }
    else {
      return field_create_instance($final_field_data);
    }
  }

  /**
   * Creates a group.
   *
   * @param array $group_data
   *   An associative array containing all of the data for this group.
   * @param string $entity_type
   *   The entity type to attach this group to.
   * @param string $bundle
   *   The bundle to attach this group to.
   * @param string $group_name
   *   The group's name.
   * @param array $headers
   *   An associative array where the keys are the header names and the values are
   *   the header indices.
   *
   * @return bool
   */
  protected function initialize_group(&$group_data, $entity_type, $bundle, $group_name, $headers) {
    $final_group_data = array();
    foreach (array_keys($headers) as $group_property_name) {
      drupal_alter('csv2fields_group_' . $group_property_name, $final_group_data, $group_data);
    }
    drupal_alter('csv2fields_group', $final_group_data, $group_data);
    if (isset($final_group_data['errors']) && count($final_group_data['errors'])) {
      foreach ($final_group_data['errors'] as $error) {
        drush_log(dt('@error on row @row_number', array(
          '@row_number' => $group_data['row_number'],
          '@error' => $error,
        )), 'error');
      }
      return FALSE;
    }
    else {
      $group = (object) array(
        'identifier' => dt('@fieldName|@type|@bundle|form', array(
          '@fieldName' => $group_data['field_group_name'],
          '@type' => $entity_type,
          '@bundle' => $bundle,
        )),
        'group_name' => $group_data['field_group_name'],
        'entity_type' => $entity_type,
        'bundle' => $bundle,
        'mode' => 'form',
        'label' => $final_group_data['label'],
        'children' => array(),
        'weight' => $final_group_data['widget']['weight'],
        'format_type' => 'fieldset',
        'format_settings' => array(
          'formatter' => 'collapsible',
          'instance_settings' => array(
            'description' => '',
            'classes' => '',
            'required_fields' => 1,
          ),
        ),
      );
      field_group_group_save($group);
      ctools_include('export');
      ctools_export_crud_enable('field_group', $group->identifier);
      return $group;
    }
  }

  /**
   * @param string $element_name
   *   The name of the field or group to add.
   * @param string $field_group
   *   The name of the field group to add to.
   * @param string $entity_type
   *   The name of the entity.
   * @param string $bundle
   *   The name of the bundle.
   */
  protected function add_element_to_group($element_name, $field_group, $entity_type, $bundle) {
    $groups = field_group_read_groups(array(
      'entity_type' => $entity_type,
      'bundle'      => $bundle,
      'view_mode'   => 'full',
    ));
    $group = $groups[$entity_type][$bundle]['form'][$field_group];
    if (!empty($group)) {
      $group->children[] = $element_name;
      field_group_group_save($group);
    }
    else {
      drush_log("Couldn't find the group @group", array(
        '@group' => $field_group,
      ), 'error');
    }
  }

  /**
   * Converts a csv row array into an associative array based on the column headers.
   *
   * @param $field_data
   *   The csv row array that contains the data.
   * @param $headers
   *   An associative array where the column name is the key, and the column number
   *   is the value.
   *
   * @return array
   *   An associative array where each column is the key and the data for the row
   *   is the value.
   */
  protected function get_data($field_data, $headers) {
    $final_field_data = array();
    foreach ($headers as $property_name => $index) {
      $final_field_data[$property_name] = trim($field_data[$index]);
    }
    return $final_field_data;
  }
}
