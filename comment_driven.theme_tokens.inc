<?php
// $Id$

function theme_comment_driven_simple_table($rows = array()) {
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
  $output = '<ul>';
  foreach ($rows as $row) {
    $output .= '<li>' . implode(' ', $row) . '</li>';
    $output .= '</li>';
  }
  $output .= '</ul>';
  return $output;
}
