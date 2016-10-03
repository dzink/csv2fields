<?php

/**
 * Gets lines from the csv.
 *
 * @param string $csv
 *   The path to a csv to read
 * @param string $subset
 *   The subset string from cli options. Should look something like '138:187'.
 *
 * @return array
 *   An array of each row of the csv. The header row has a key of 'header_row',
 *   while every other row's key is an integer.
 */
function _csv2fields_get_csv_lines(&$csv, $header_row = 1, $subset = '') {
  $fields = array();
  $row = 1;
  if (!empty($subset)) {
    $subset = explode(':', $subset);
    $start_line = $subset[0];
    $end_line = $subset[1];
  }
  if (($handle = fopen($csv, "r")) !== FALSE) {
    while (($data = fgetcsv($handle, 1000, ",")) !== FALSE) {
      if ($row == $header_row) {
        $fields['header_row'] = $data;
      }
      elseif (($row >= $start_line && $row <= $end_line) || empty($subset)) {
        $fields[$row] = $data;
      }
      $row++;
    }
    fclose($handle);
  }
  return $fields;
}
