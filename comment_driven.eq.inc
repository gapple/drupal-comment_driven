<?php
// $Id$

/**
 * This module uses equality only for two dispensable purposes
 * - allowing empty comment only if there were some changes made
 * - displaying the differences introduced by each comment
 * both of this features rely on change detection
 * which is the "Achilles' heel" of this module
 * being the most "per case basis" code and therefore error-prone
 * also knowing 3rd party module's tweaks is needed to support their equality
 * 
 * Moreover, this is the only reason for this module to have a schema,
 * almost deserving its own sub-module space, but having comment driven content 
 * makes no sense without displaying the difference introduced by each comment
 * 
 * And most of all:
 * change detection is required to avoid saving the node for every posted comment
 * which is unaffordable when revisions are turned on
 * 
 * PS: every function in this file is only reachable through _comment_driven_elem_changes
 */

// $element is recommended to be a whole $form
// it supports being an isolated $element,
// but some kinds of elements aren't supported
// i.e. those having no #array_parents
// e.g. #type: token, radio
// Note: $complete_form is passed by ref only to avoid
// several array copies in vain (it isn't meant to be modified in any way)
// it is only used to find out $parents_titles for labels
function _comment_driven_elem_changes($node_type, $element_kind, $element, $form_values, &$complete_form) {
  $changes = array();
  
  // element['#access'] must be TRUE or not set

  // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
  if ($element['#access'] === FALSE) comment_driven_assertion_failed('#access===FALSE');
  
  // elements having not #default_value should not be processed
  // e.g. forms, fieldset, wrappers, etc
  if ($element_kind == 'wrapper') {
    foreach (element_children($element) as $key) {
      // avoid to ask the element_kind of inaccesible elements (e.g. some form values, hiddens)
      // also ingore diguised comment form elements
      if ($element[$key]['#access'] === FALSE || strpos($key, 'driven:') === 0) {
        continue;
      }
      
      // @TODO: review why #type=>radio has no #array_parents
      if (empty($element[$key]['#array_parents']) && in_array($element[$key]['#type'], array('radio', 'radios'))) {
      // can't ask $child_kind before having proper #array_parents
      //if (empty($element[$key]['#array_parents']) && !in_array($child_kind, array('wrapper', 'bypass', 'nested'))) {
        $element[$key]['#array_parents'] = $element['#array_parents'] + array($key);
      }
      
      $child_kind = comment_driven_element_kind($node_type, $element[$key]);
      
      $changes += _comment_driven_elem_changes($node_type, $child_kind, $element[$key], $form_values, $complete_form);
  
      // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
      if (!is_array($changes)) comment_driven_assertion_failed('!is_array($changes) while processing $key: ' . $key);
    }
  }
  elseif ($element_kind != 'bypass' && $element_kind != 'nested') {
    // $value will be passed by ref to comment_driven_[element_kind]_equal functions
    // to be normalized in one round on per case basis
    // the existing problem:
    // #default_value & $form_state['values'] don't match
    // they might differ in other ways beside the actual change
    // e.g. some arrays are compared by their values
    // therefore non-assoc arrays vs assoc arrays might be equal despite their keys
    // differences between their keys should be ignored in some cases      
    // this becomes worse when #default_value is a non-assoc array
    // while $form_state['values'] is a plain number
    // and unleashed when CCK comes in
    $value = _comment_driven_form_get_value($element['#parents'], $form_values);
    
    // @TODO: document this function name pattern
    // which namespace is under 'comment_driven_' namespace
    // no matter what the sub-module name might be 
    $function = 'comment_driven_' . $element_kind . '_equal';
    $meta = array();
    if (!($function($element, $value, $meta))) {
      $parents_titles = array();
      $parent = &$complete_form; // avoid array copy (performance)
      foreach ($element['#array_parents'] as $key) {
        $parent = &$parent[$key]; // avoid array copy (performance)
        $parents_titles[] = isset($parent['#title']) ? $parent['#title'] : '';
      }
      $function = 'comment_driven_' . $element_kind . '_label';
      if (function_exists($function)) {
        $label = $function($element, $parents_titles);
      }
      else {
        $label = comment_driven_first_title($element, $parents_titles);
      }
      
      $changes[$element_kind . ':' . implode(':', $element['#parents'])] = array(
        'label' => $label,
        'old' => $element['#default_value'],
        // $value comes normalized from $form_state['values'] to #default_value
        // done in comment_driven_[element_kind]_equal functions on per case basis
        'new' => $value,
        'meta' => $meta,
      );
    }
  }
  
  return $changes;
}

// special cases:
// - wrapper: don't analyze it, but pass its children through
// - bypass: don't analyze it, don't pass its children neither
// - nested: same as 'bypass', but note that its parent was indeed analysed
function comment_driven_element_kind($node_type, $element) {
  $property_kinds = comment_driven_property_kinds($node_type);
  
  // @TODO: review why #type=>textarea has no #default_value (e.g. node:body_field:body)
  if (isset($element['#default_value']) && !in_array($element['#type'], array('token'))) {
    // @TODO: review why #type=>radio has no #array_parents
    // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
    if (empty($element['#array_parents'])) comment_driven_assertion_failed('empty #array_parents for #type: ' . $element['#type']);
    
    // take into account #array_parents
    // for instance teaser_js, teaser_include, body, format
    // all of them are node:body_field:*
    // also status, sticky, promote belongs to node:options:*
    // Note: #parents has the path to get the value from $form_status['values']
    // while #array_parents has the path to get the $element in the $form
    $element_path = implode(':', $element['#array_parents']);
    if (isset($property_kinds[$element_path])) {
      $element_kind = $property_kinds[$element_path];
    }
    else {
      // look for nested properties (e.g. node:body_field:*, cck:field_name:*)
      foreach (array_keys($property_kinds) as $property_path) {
        if (strpos($element_path, $property_path . ':') === 0) {
          $element_kind = $property_kinds[$property_path];
          // @TODO: document this function name pattern
          // which namespace is under 'comment_driven_' namespace
          // no matter what the sub-module name might be 
          $function = 'comment_driven_' . $element_kind . '_bypass_property';
          if ($function($node_type, $element_path, $element)) {
            $element_kind = 'nested';
          }
          break; 
        }
      }   
    }
  }
  else {
    // passed through elements are either:
    // * without value (i.e. #default_value not set, e.g. fieldset, wrapper)
    // * FAPI internal use (e.g. #type: token, some form hiddens)
    $element_kind = isset($element['#value']) ? 'bypass' : 'wrapper';
  }
  
  if (!isset($element_kind)) {
    // having access to an unknown driven property?!
    // how could it be?
    
    // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
    comment_driven_assertion_failed('$element_kind = unknown');
    
    $element_kind = 'unknown';
    drupal_set_message(t('An forbidden choice has been detected. Please contact the site administrator.'), 'error', FALSE);
    watchdog('comment_driven', 'Forbidden element "!name" (with value "%value") was accesible.', array('!name' => empty($element['#title']) ? $element['#parents'][0] : $element['#title'], '%value' => $value), WATCHDOG_ERROR);
  }
  
  return $element_kind;
}


/**
 * Implements comment_driven_[element_kind]_equal().
 */
function comment_driven_unknown_equal($element, &$value) {
  // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
  comment_driven_assertion_failed('unknown $element_kind');
  
  // do the best possible (not considering arrays, since might require recursive comparison)
  return _comment_driven_equal_non_array($element, $value); 
}

/**
 * Implements comment_driven_[element_kind]_label().
 */
//function comment_driven_unknown_label($element, $parents_titles) {
//  return comment_driven_first_title($element, $parents_titles);
//}

function comment_driven_first_title($element, $parents_titles) {
  if (!empty($element['#title'])) return $element['#title'];
  $parents_titles = array_filter($parents_titles);
  return array_pop($parents_titles);
}

// Note that both arguments are passed by value
// therefore if $value is passed it won't be affected in any way
function _comment_driven_equal_array_values($arr1, $arr2) {
  // using count, since empty won't receive direct result from array_diff
  return (0 == (count(array_diff($arr1, $arr2)) + count(array_diff($arr2, $arr1))));
}

// Note that both arguments are passed by value
// therefore if $value is passed it won't be affected in any way
function _comment_driven_equal_keyed_array_in_depth($arr1, $arr2) {
  if (!_comment_driven_equal_array_values(array_keys($arr1), array_keys($arr2))) {
    return FALSE;
  }
  foreach ($arr1 as $key => $value) {
    // $arr1[$key] might be NULL, 
    // and isset($arr2[$key]) checked bellow
    // would return FALSE if it is NULL as well
    if (!isset($arr1[$key])) {
      if (!isset($arr2[$key])) {
        // neither are set
        continue;
      }
      else {
        return FALSE;
      }
    }
    if (!isset($arr2[$key]) || (is_array($arr1[$key]) != is_array($arr2[$key]))) {
      return FALSE;
    }
    if (is_array($arr1[$key])) {
      if (!_comment_driven_equal_keyed_array_in_depth($arr1[$key], $arr2[$key])) {
        return FALSE;
      }
    }
    else {
      if (!_comment_driven_equal_non_array(array('#default_value' => $arr1[$key], '#type' => 'dummy'), $arr2[$key])) {
        return FALSE;
      }
    }
  }
  return TRUE;
}

// do the best possible (not considering arrays, since might require recursive comparison) 
function _comment_driven_equal_non_array($element, &$value) {
  // $value will be normized by if needed (per case basis)
  // to match #default_value format instead of $form_state['values']
  
  $default = $element['#default_value']; 
  if (is_bool($default) && is_int($value)) { 
    $value = (bool)$value; 
  }
  elseif (is_int($default) && is_bool($value)) {
    $value = (int)$value;
  }
  elseif (is_string($default) && (is_bool($value) || is_int($value))) {
    $value = (string)$value;
  }
  
  // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
  if (!in_array($element['#type'], array('checkbox', 'radio', 'radios', 'textfield', 'textarea'))) {
    comment_driven_assertion_failed('missing case for #type: ' . $element['#type']);
  }
  if (gettype($default) != gettype($value)) comment_driven_assertion_failed('#type: ' . $element['#type'] . '; gettype($default) != gettype($value): ' . gettype($default) . ' vs ' . gettype($value));
  
  // using === to avoid comparing zero vs string which gives TRUE
  return ($default === $value || (empty($default) && empty($value)));
}

// first pick up values
// then deeply compare them
function _comment_driven_symmetric_diff($arr1, $arr2) {
  $outgoing = $arr1;
  $incoming = $arr2;
  
  foreach ($arr1 as $index1 => $value1) {
    foreach ($arr2 as $index2 => $value2) {
      if (is_array($value1) && is_array($value2)) {
        if (_comment_driven_equal_keyed_array_in_depth($value1, $value2)) {
          unset($outgoing[$index1]);
          unset($incoming[$index2]);
          break;
        }
      }
      elseif (!is_array($value1) && !is_array($value2)) {
        if (_comment_driven_equal_non_array(array('#default_value' => $value1, '#type' => 'dummy'), $value2)) {
          unset($outgoing[$index1]);
          unset($incoming[$index2]);
          break;
        }
      }
      // else, they are not equal, continue looking
    }
  }
  
  // reset keys
  return array(array_values($outgoing), array_values($incoming));
}

function comment_driven_property_kinds($node_type) {
  static $property_kinds = array();
  
  if (!isset($property_kinds[$node_type])) {
    $property_kinds[$node_type] = array();
    $driven_props = variable_get('comment_driven:properties_' . $node_type, array());

    foreach ($driven_props as $driven_prop) {
      $property_path = explode(':', $driven_prop);
      $property_kind = array_shift($property_path); // remove property kind prefix
      
      $property_kinds[$node_type][implode(':', $property_path)] = $property_kind;
    }
  }
  
  return $property_kinds[$node_type];
}

// see form_set_value at form.inc
function _comment_driven_form_get_value($parents, $form_values) {
  $parent = array_shift($parents);
  if (empty($parents)) {
    $value = $form_values[$parent];
  }
  else {
    if (isset($form_values[$parent])) {
      $value = _comment_driven_form_get_value($parents, $form_values[$parent]);
    }
    else {
      $value = NULL;
    }
  }
  return $value;
}
