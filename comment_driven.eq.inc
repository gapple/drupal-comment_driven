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
 */

// shouldn't be directly invoked (use comment_driven_form_changes instead)
// $element is recommended to be a whole $form
// it supports being an isolated $element,
// but some kinds of elements aren't supported
// i.e. those having no #array_parents
// e.g. #type: token, radio
function _comment_driven_elem_changes($driven_props, $element, $form_values, $ascendant_title) {
  $changes = array();
  
  // element['#access'] must be TRUE or not set

  // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
  if ($element['#access'] === FALSE) comment_driven_assertion_failed('#access===FALSE');
  
  // note that 'form' will be always passed through
  // since it is only expected as the root element in a form
  // (do XHTML specifications allow for nested forms?)
  // nevermind, Drupal doesn't support nested forms
  // see static $complete_form on top of form_builder
  
  // the first time the $form doesn't have #array_parents
  // but that shouldn't be the criteria to discriminate,
  // some other elements might come up without #array_parents (e.g. radio)
  if ($element['#type'] != 'form') {
    // the form_build_id is not he only hidden with #value and no #default_value
    // hierarchical_select has its own hs_form_build_id
    if (empty($element['#array_parents']) && $element['#type'] == 'hidden') {
      return array(); // step out
    }
    
    // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
    if (empty($element['#array_parents'])) comment_driven_assertion_failed('empty #array_parents for #type: ' . $element['#type']);
      
    // @TODO: review why #type=>radio has no #array_parents
    if (empty($element[$key]['#array_parents']) && in_array($element[$key]['#type'], array('radio', 'radios'))) {
      $element[$key]['#array_parents'] = $element['#array_parents'] + array($key);
    }

    // Note: #parents has the path to get the value from $form_status['values']
    // while #array_parents has the path to get the $element in the $form
    $property_path = implode(':', $element['#array_parents']);
    $prop = isset($driven_props[$property_path]) ? $driven_props[$property_path] : FALSE;
  }
  
  if ($prop) {
    // modules should be aware of mismatching #default_value & $form_state['values']
    // they might differ in other ways beside the actual change
    // e.g.
    // - some arrays are compared by their values
    //   therefore non-assoc arrays vs assoc arrays might be equal despite their keys
    //   differences between their keys should be ignored in some cases      
    // - this becomes worse when #default_value is a non-assoc array
    //   while $form_state['values'] is a plain number
    // - and unleashed when CCK comes in
    // therefore, upon comparison, they have the chance to alter the $value
    // normalizing it to match #default_value format easing diff_render implementation
    // without re-taking into account the same altertanives than diff_values did
    $function = $prop['module'] . '_diff_values';
    $meta = $prop['meta'];
    $diff_values = $function($element, $form_values, $meta);
    if ($diff_values) {
      $label = empty($prop['label']) ? (empty($element['#title']) ? $ascendant_title : $element['#title']) : $prop['label'];
      list($old, $new) = $diff_values;
      
      // properties might no longer exist suddenly
      // a backup render has to be made instantly and kept
      // but until this point we might be just validating
      // a default $label will be needed for diff_render time
      // ($old & $new will also be needed for diff_render time, but they storage is unquestionable)
      $changes[$property_path] = compact('label', 'old', 'new');
      // nevertheless, validator ended up rendering diff for preview
      // then $diff_render is being computed on every validation (being save or preview)
      // and not saved for submission, which will have to compute $diff_render again
      // BUT, won't paid a minor performance improvement in detriment of clean logic
      // asking for $changes doesn't involves render beyond
      // easing default label (relying on ascendant title)
      // since validator doesn't requires to preview the diff summary
      // that being optional makes sense to keep diff_render in submission stage
    }
  }
  else {
    // if it is not a driven property, pass through it
    foreach (element_children($element) as $key) {
      // bypass inaccesible elements (e.g. some form values, hiddens)
      // also ingore diguised comment form elements
      if ($element[$key]['#access'] === FALSE || strpos($key, COMMENT_DRIVEN__DISGUISE_PREFIX) === 0) {
        continue;
      }
      
      $changes += _comment_driven_elem_changes($driven_props, $element[$key], $form_values, (empty($element['#title']) ? $ascendant_title : $element['#title']));
      
      // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
      if (!is_array($changes)) comment_driven_assertion_failed('!is_array($changes) while processing $key: ' . $key);
    }
  }
  
  return $changes;
}

function comment_driven_equal_array_values($arr1, $arr2) {
  // using count, since empty won't receive direct result from array_diff
  return (0 == (count(array_diff($arr1, $arr2)) + count(array_diff($arr2, $arr1))));
}

function comment_driven_equal_keyed_array_in_depth($arr1, $arr2) {
  if (!comment_driven_equal_array_values(array_keys($arr1), array_keys($arr2))) {
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
      if (!comment_driven_equal_keyed_array_in_depth($arr1[$key], $arr2[$key])) {
        return FALSE;
      }
    }
    else {
      if (!_comment_driven_equal_non_array($arr1[$key], $arr2[$key])) {
        return FALSE;
      }
    }
  }
  return TRUE;
}

// do the best possible (not considering arrays, since might require recursive comparison)
function _comment_driven_equal_non_array($default, &$value) {
  // $value will be normized if needed (per case basis)
  // to match #default_value format instead of $form_state['values']
  
  // can't know the intended type for NULL
  // the caller function must be reponsible of normalization
  // (e.g. case 'select' in comment_driven_taxo_diff_values)
  if (is_null($default) || is_null($value)) {
    return (is_null($default) && is_null($value));
  }
  
  if (is_bool($default) && is_int($value)) { 
    $value = (bool)$value; 
  }
  elseif (is_int($default) && (is_bool($value) || is_numeric($value))) {
    $value = (int)$value;
  }
  elseif (is_string($default) && (is_bool($value) || is_int($value))) {
    // not considering is_float($value) because
    // in some cases driven properties would need to take care
    // of converting decimal values to string to match
    // decimal zeros on the extreme right (e.g. CCK's decimal)
    // which would be very tricky for elements nested into arrays
    // therefore, this function doesn't support float value
    // which would need to reach this point already converted to string
    $value = (string)$value;
  }
  
  if (gettype($default) != gettype($value)) comment_driven_assertion_failed('gettype($default) != gettype($value): ' . gettype($default) . ' vs ' . gettype($value));
  
  // using === to avoid comparing zero vs string which gives TRUE
  return ($default === $value || (empty($default) && empty($value)));
}

// note that symmetric_diff will consider discard keys for first level
// first pick up values and then deeply compare them
// and finally will return arrays with reseted keys
// (avoid direct invokation, use comment_driven_symmetric_diff instead)
function _comment_driven_symmetric_diff($arr1, $arr2) {
  // we are going to iterate n^2
  // but that doesn't make clear at first sight
  // that keys are being discarded
  // so, lets make it clear enough
  $arr1 = array_values($arr1);
  $arr2 = array_values($arr2);
  
  $outgoing = $arr1;
  $incoming = $arr2;
  
  foreach ($arr1 as $index1 => $value1) {
    foreach ($arr2 as $index2 => $value2) {
      if (is_array($value1) && is_array($value2)) {
        if (comment_driven_equal_keyed_array_in_depth($value1, $value2)) {
          unset($outgoing[$index1]);
          unset($incoming[$index2]);
          break;
        }
      }
      elseif (!is_array($value1) && !is_array($value2)) {
        if (_comment_driven_equal_non_array($value1, $value2)) {
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

// see form_set_value at form.inc
function comment_driven_form_get_value($parents, $form_values) {
  $parent = array_shift($parents);
  if (empty($parents)) {
    $value = $form_values[$parent];
  }
  else {
    if (isset($form_values[$parent])) {
      $value = comment_driven_form_get_value($parents, $form_values[$parent]);
    }
    else {
      $value = NULL;
    }
  }
  return $value;
}
