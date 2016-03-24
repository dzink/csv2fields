<?php

/**
 * Implements hook_drush_command().
 */
function csv2fields_drush_command() {
  $items['gghh-csv2fields-import'] = array(
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
 * @param $type - The content type to add fields to
 * @param $bundle - The bundle to add fields to
 * @param $csv - The path to a csv to read
 */
function drush_csv2fields_import($type, $bundle, $csv) {
  $subset = drush_get_option('subset', FALSE);
  $header_row = drush_get_option('header-row', 1);
  print dt($type) . PHP_EOL;
  print dt($csv) . PHP_EOL;
  $fields = _csv2fields_get_csv_lines($csv, $header_row, $subset);
  $header_row = $fields['header_row'];
  unset($fields['header_row']);
  $map = _csv2fields_initialize_mapping($header_row);
  $all_fields = field_info_field_map();
  foreach ($fields as $row_number => $field_data) {
    if (!empty($field_data[$map['machine_name']]) && !empty($field_data[$map['type']])) {
      print 'x: ' . $field_data[$map['machine_name']] . PHP_EOL;
      $field = _csv2fields_add_field($field_data, $all_fields, $map);
      $field_instance = _csv2fields_add_to_entity($field, $field_data, $all_fields, $map, $type, $bundle, $row_number);
    }
  }
}
