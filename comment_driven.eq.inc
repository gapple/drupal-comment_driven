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

// deferred implementation of comment_driven_diff_node_state
function _comment_driven_diff_node_state($ned_form, $form_values, $driven_props) {
  // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
  if ($ned_form['#access'] === FALSE) comment_driven_assertion_failed('$ned_form[\'#access\']===FALSE');
/*  if (FALSE === $ned_form['#access']) {
    // access denied => won't consent any difference
    return array();
  }*/
  
  // IMPORTANT: is responsibility of the caller function
  // to check $ned_form['#access'] (just top level)
  // if desired/intended this function will work beneath a node_form with #access=FALSE
  
  // this is only needed to improve error reporting (warn_property_gone)
  $node_type = $ned_form['#node']->type;
  
  $changes = array();
  $property_paths = array_keys($driven_props);
  foreach ($property_paths as $property_path) {
    $path_parts = explode(':', $property_path);
    $element = $ned_form;
    foreach ($path_parts as $key) {
      if (empty($element[$key])) {
        // unable to navigate beneath it

        // this might be possible due to properties moved or gone
        // and content type form not re-submitted
        // (e.g. fieldgroup/multigroup movement or disabling modules)
// @TODO: cck realm might look if the property was moved and exists under a different path for the same content type
        comment_driven_warn_property_gone($property_path, $node_type);
        
        // comparison is not possible => can't return any difference
        return array();
      }
      
      $element = $element[$key];
      
      if (!empty($element['#title'])) {
        $ancestor_title = $element['#title'];
      }
    }
        
    $prop = $driven_props[$property_path];
    $meta = $prop['meta'];
    $diff_values = comment_driven_elem_diff_values($element, $form_values, $meta);
    if ($diff_values) {
      list($old, $new) = $diff_values;
      $label = empty($prop['label']) ? $ancestor_title : $prop['label'];
      
      // properties might no longer exist suddenly
      // a backup render has to be made instantly and kept
      // but until this point we might be just validating
      //
      // a default $label will be needed for diff_render time
      // ($old & $new will also be needed for diff_render time, but they storage is unquestionable)
      $changes[$property_path] = compact('label', 'old', 'new');
      // nevertheless, validator ended up rendering diff for preview
      // then $diff_render is being computed on every validation (being save or preview)
      // and not saved for submission, which will have to compute $diff_render again
      // BUT, won't go for a minor performance improvement in detriment of clean logic
      // asking for $changes doesn't involves render
      // beyond easing default label (relying on ancestor title)
      // since validator doesn't requires to preview the diff summary
      // that being optional makes sense to keep diff_render in submission stage
    }
  }
  return $changes;
}

function comment_driven_elem_diff_values($element, $form_values, $meta) {
  // Access Control Policies SHOULD be already applied using comment_driven_access_control
  // check #access flag
  if (FALSE === $element['#access']) {
    // access denied => won't consent any difference
    return FALSE;
  }
  
  // wrapper detection
  // if values are not at this level pass through it
  // careful, isset is not enough, since it can be NULL (e.g. number)
  // not using in_array, since it compares and matches zero with string
  if (!comment_driven_has_default_value_key($element)) {
    if (!count(element_children($element))) {
      // having no children is a dead end (e.g. add_more)
      return FALSE; // ignore it
    }
    
    $element_children = element_children($element);
    
    // while being a wrapper, it can hold expanded radios (e.g. format is a fieldset)
    // if so, each of its children is expected to be radio (or wrapper, e.g. format tips)  
    foreach ($element_children as $key) {
      if ($element[$key]['#type'] === 'radio') {
        // then either of its children will suffice to pick up the #default_value
        // and we have one at hand already
        return comment_driven_elem_diff_values($element[$key], $form_values, $meta);
      }
    }
      
    $equal_keys = array();
    foreach ($element_children as $key) {
      $diff_values = comment_driven_elem_diff_values($element[$key], $form_values, $meta);
      if (!$diff_values) {
        // keep track
        // since later isset might not be enough if $olds[$key]=NULL 
        $equal_keys[] = $key;
        continue;
      }
      
      list($old, $new) = $diff_values;
      // on diff_render symmetric_diff will drop the keys
      // therefore reseted indexes wouldn't harm here
      // but wouldn't be accurate either (e.g. for debugging)
      // in addition, content_taxonomy handles the key 'tids'
      // which this way is persisted into diff_values
      // and later recognized in diff_render time
      $olds[$key] = $old;
      $news[$key] = $new;
    }
    
    if (empty($olds) && empty($news)) return FALSE;
    // IMPORTANT: wrapper_joint is in depth (recursive)
    if ($meta['wrapper_joint']) {
      // property requested to include non changed children
      // if at least one of them changed
      // since all of them will be needed at diff_render time
      // IMPORTANT: every children has to have known_default_value
      // (i.e. $element['#default_value'] or a child of #type=radio)
      // otherwise they will be ignored (e.g. format tips)
      foreach ($equal_keys as $key) {
        // the only strategy known for wrapper_joint
        // would be the clasic #default_value vs comment_driven_form_get_value
        // but there is not need to keep track of normalized values
        // since they were declared equal to #default_value
        //
        // if a potential wrapper is found it will be bypassed (e.g. format tips)
        if (comment_driven_has_default_value_key($element[$key])) { 
          $news[$key] = $olds[$key] = comment_driven_known_default_value($element[$key]);
        }
      }
    }
    return array($olds, $news);
  }
  // hitting a non-wrapper beneath the property path
  
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
  $function = $meta['module'] . '_diff_values';
  $diff_values = $function($element, $form_values, $meta);
  
  return $diff_values;
}

function comment_driven_equal_array_values($arr1, $arr2) {
  // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage
  foreach (array_values($arr1) + array_values($arr2) as $value) { 
    if (is_array($value)) comment_driven_assertion_failed('misusage or equal_array_values comparison: nested array within arguments');
  }
  
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

// deferred implementation of comment_driven_symmetric_diff
function _comment_driven_symmetric_diff($arr1, $arr2) {
  // note that symmetric_diff will discard keys for first level
  // first pick up values and then deeply compare them
  // and finally will return arrays with reseted keys
    
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

// helper function to pick up the default value
// ONCE KNOWN: it exists or one of its children is #type=radio
// i.e. $element['#default_value'] or from a child of #type=radio
function comment_driven_known_default_value($element) {
  if (comment_driven_has_default_value_key($element)) {
    return $element['#default_value'];
  }
  // still, while being a wrapper, it can hold expanded radios (e.g. format is a fieldset)
  // if so, each of its children is expected to be radio (or wrapper, e.g. format tips)  
  foreach (element_children($element) as $key) {
    // finding children with no defualt value isn't enough for a negative
    // since they can be wrappers (e.g. format tips)
    if (comment_driven_has_default_value_key($element[$key])) {
      // found a non-wrapper => its type being radio would be a positive (while negative otherwise)
      if ($element[$key]['#type'] === 'radio') {
        // then either of its children will suffice to pick up the #default_value
        // and we have one at hand already
        return $element[$key]['#default_value'];
      }
      // found a children with default value but it isn't a radio
      // then stop looping through its children, it is already a negative
      
      // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
      comment_driven_assertion_failed('misusage of known_default_value: $element has no #default_value and neither or its children is #type=radio');
      
      return FALSE;
    }
  }
  
  // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
  comment_driven_assertion_failed('misusage of known_default_value: $element has no #default_value and neither or its children is #type=radio');
  
  // if this point is reached then either
  // - it had no children
  // - its children didn't had default value
  return FALSE;
} 

// helper function for wrapper detection
function comment_driven_has_default_value_key($element) {
  // this is the heart of the so called "wrapper detection"
  // it is not all of it, since it might be needed
  // to check for count(element_children($element) as well
  //
  // we only care if #default_value exists as a key of $element
  // not whether it is empty/NULL or anything, it doesn't matters
  //
  // careful, isset is not enough, since it can be NULL (e.g. number)
  // in_array is not enough, since it compares and matches zero with string
  // therefore, discard any zero key (or empty keys in general)
  // (there are not keys being FALSE/NULL/'', but if so, they are intended to be removed as well)
  $keys = array_filter(array_keys($element));
  return (in_array('#default_value', $keys));
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
