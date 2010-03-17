<?php
// $Id$

function theme_comment_driven_simple_table($rows = array()) {
  // avoid printing tags
  if (empty($rows)) return '';
  
  $output = '<table><tbody>';
  foreach ($rows as $row) {
    $output .= '<tr>';
    foreach ($row as $cell) {
      $output .= '<td>' . $cell . '</td>';
    }
    $output .= '</tr>';
  }
  $output .= '</tbody></table>';
  return $output;
}

function theme_comment_driven_simple_ulist($rows = array()) {
  // avoid printing tags
  if (empty($rows)) return '';
  
  $output = '<ul>';
  foreach ($rows as $row) {
    $output .= '<li>' . implode(' ', $row) . '</li>';
  }
  $output .= '</ul>';
  return $output;
}
