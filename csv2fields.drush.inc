<?php

/**
 * Implements hook_drush_command().
 */
function csv2fields_drush_command() {
  $items['csv2fields-import'] = array(
    'description' => 'Import fields from a csv into a content type.',
    'aliases' => array('c2f'),
    'arguments' => array(
      'content_type' => 'The content type to add fields to.',
      'bundle' => 'The bundle to add fields to.',
      'csv' => 'The path to a csv to read.',
    ),
    'options' => array(
      'subset' => 'Lines to get from the csv file in the format start_line:end_line.',
      'header-row' => 'The row that contains the headers.',
    ),
  );
  return $items;
}

/**
 * @param $entity_type - The content type to add fields to
 * @param $bundle - The bundle to add fields to
 * @param $csv - The path to a csv to read
 */
function drush_csv2fields_import($entity_type, $bundle, $csv) {
  $subset = drush_get_option('subset', FALSE);
  $header_row = drush_get_option('header-row', 1);
  $fields = _csv2fields_get_csv_lines($csv, $header_row, $subset);
  $headers = $fields['header_row'];
  unset($fields['header_row']);
  $headers = array_filter($headers);
  $headers = array_flip($headers);
  foreach ($fields as $row_number => $field_data) {
    $field_data = _csv2fields_get_data($field_data, $headers);
    $field_data['row_number'] = $row_number;
    if (!isset($field_data['field_group_name']) || empty($field_data['field_group_name'])) {
      $field_info = csv2fields_initialize_field($field_data, $headers);
      if ($field_info) {
        $field_name = $field_info['field_name'];
        $field_data['#field_info'] = $field_info;
        $field_instance = csv2fields_initialize_instance($field_data, $entity_type, $bundle, $field_name, $headers);
        if (!empty($field_data['field_group_parent'])) {
          csv2fields_add_element_to_group($field_name, $field_data['field_group_parent'], $entity_type, $field_instance['bundle']);
        }
      }
    }
    else {
      csv2fields_initialize_group($field_data, $entity_type, $bundle, $group_name, $headers);
      $group_name = $field_data['field_group_name'];
      if (!empty($field_data['field_group_parent'])) {
        csv2fields_add_element_to_group($group_name, $field_data['field_group_parent'], $entity_type, $group_date['bundle']);
      }
    }
  }
}
