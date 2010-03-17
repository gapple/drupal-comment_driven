<?php
// $Id$

module_load_include('inc', 'comment_driven', 'comment_driven.eq');

function comment_driven_inspect_diff_values_refresh($old_vid, $new_vid, &$errors = array(), &$node_type = NULL) {
  // we receive $node_type by ref to be used as an optional output
  // so the caller be aware of what content type was matched (if there weren't fatal errors) 
  //
  // nids might differ if desired/intended, it will be supported
  // but $old_vid and $new_vid MUST belong to the same content type
  $query = 'SELECT n.nid AS nid, n.type AS type, nrev.vid AS vid'
    . ' FROM {node_revisions} nrev'
    . ' LEFT JOIN {node} n'
    . ' ON nrev.nid = n.nid'
    . ' WHERE nrev.vid = %d';
  foreach (array($old_vid, $new_vid) as $vid) {
    $result = db_query($query, $vid);
    $row = db_fetch_array($result);
    if ($row) {
      $nids[$vid] = $row['nid'];
      $node_types[$vid] = $row['type'];
    }
    else {
      $errors[] = t('Could not fetch revision:') . ' ' . $vid;
      // fatal error, can't proceed with any kind of comparison
      return FALSE;
    }
  }
  if ($node_types[$old_vid] != $node_types[$new_vid]) {
    $errors[] = t('Revisions belong to different content types:') . ' ' . $node_types[$old_vid] . ' vs ' . $node_types[$new_vid];
    // fatal error, can't proceed with any kind of comparison
    return FALSE;
  }
  
  // following checks are non fatal errors
  // since we can perform the comparison
  if ($nids[$old_vid] != $nids[$new_vid]) {
    $errors[] = t('Compared revisions belong to DIFFERENT nodes:') . ' ' . $nids[$old_vid] . ' vs ' . $nids[$new_vid];
  }
  elseif ($old_vid == $new_vid) {
    $errors[] = t('Comparing a revision to itself.');
  }
  elseif ($old_vid > $new_vid) {
    $errors[] = t('Compared revisions are in REVERSE chronological order.');
  }

  // both content types matched on previous validation
  // (received $node_type as optional param to be used as an optional output)
  $node_type = $node_types[$old_vid];
  // retrieve all available properties
  $driven_props = comment_driven_available_properties($node_type);
  
  $old_node = node_load($nids[$old_vid], $old_vid);
  $new_node = node_load($nids[$new_vid], $new_vid);
  
  $changes = comment_driven_inspect_diff_nodes($old_node, $new_node, $driven_props);

  return $changes;
}

function comment_driven_inspect_build_node_form($node, $driven_props = NULL) {
  $node_type = $node->type;
  // include required files to manually guide a node_form flow
  module_load_include('inc', 'node', 'node.pages');
  if (module_exists('content')) {
    module_load_include('inc', 'content', 'includes/content.node_form');
  }

  // we are about to prepare a node_form which might be modified
  // by 3rd party modules, such form contains terms (and other entities/data)
  // that might be statically cached and shared between different nodes
  // this function is intended to be invoked with different node revisions
  // (two of them at least) during the process of a single form
  // therefore, form_alter acting on the first node_form might provoke
  // entity changes that will be picked up by subsequent forms
  // a simple way to test this pitfall would be requesting live comparison
  // of the same revision for a node with taxonomy_single_tag
  // then the second time the form is processed the term will be double escaped
  // and, while both old_node and new_node will be affected
  // only new_node_form will reflect the double processing
  // since old_node_form was the first one
  // and was affected for the process only one time as intended
  // therefore, we perform a deep clone
  $node = comment_driven_inspect_deep_clone($node);
  
  // 'ned' prefix stands for 'node_edit'
  $ned_form_id = $node_type . '_node_form';
  $ned_form_state = array();
  $ned_form = drupal_retrieve_form($ned_form_id, $ned_form_state, $node);
  drupal_prepare_form($ned_form_id, $ned_form, $ned_form_state);

  // we are not in a form_alter,
  // therefore this form still needs to be processed
  drupal_process_form($ned_form_id, $ned_form, $ned_form_state);

  if (is_null($driven_props)) {
    $driven_props = comment_driven_get_properties($node_type);
  }
  
  // bypassing Access Control Policies to perform an accurate comparison
  // this will only benefit those who reached this point
  // (guarded by COMMENT_DRIVEN_INSPECT__PERM)
  // and it's going to be used ONLY to READ values (i.e. inspect)

  return $ned_form;
}

function comment_driven_inspect_diff_nodes($old_node, $new_node, $driven_props = NULL) {
  $old_node_form = comment_driven_inspect_build_node_form($old_node, $driven_props); 
  $new_node_form = comment_driven_inspect_build_node_form($new_node, $driven_props);

  $changes = array();
  $property_paths = array_keys($driven_props);
  // both forms must be of the same content type
  // but one of them might have children the other one doesn't have
  // therefore it is required to navigate synchronized,
  // but taking into account they might mismatch
  // by one of them (or both) missing an element
  foreach ($property_paths as $property_path) {
    $path_parts = explode(':', $property_path);
    $old_parent = $old_node_form;
    $new_parent = $new_node_form;
    foreach ($path_parts as $key) {
      $old_parent = empty($old_parent[$key]) ? array() : $old_parent[$key];
      $new_parent = empty($new_parent[$key]) ? array() : $new_parent[$key];
      
      // remember that either might not exists
      if (!empty($old_parent['#title'])) {
        $ancestor_title = $old_parent['#title'];
      }
      elseif (!empty($new_parent['#title'])) {
        $ancestor_title = $new_parent['#title'];
      }
    }
    
    $prop = $driven_props[$property_path];
    $diff_values = comment_driven_inspect_diff_values($old_parent, $new_parent, $prop);
    if ($diff_values) {
      list($old, $new) = $diff_values;
      $label = empty($prop['label']) ? $ancestor_title : $prop['label'];
      $changes[$property_path] = compact('label', 'old', 'new');
    }
  }
  return $changes;
}

function comment_driven_inspect_diff_values($old_elem, $new_elem) {
  // if values are not at this level pass through it
  // careful, isset is not enough, since it can be NULL (e.g. number)
  // not using in_array, since it compares and matches zero with string
  if (!comment_driven_has_default_value_key($old_elem) && !comment_driven_has_default_value_key($new_elem)) {
    if (!count(element_children($old_elem)) && !count(element_children($new_elem))) {
      // bypass (it should be add_more or something else)
      return FALSE;
    }
    
    $old_children_keys = element_children($old_elem); 
    $new_children_keys = element_children($new_elem); 

    foreach ($old_children_keys as $key) {
      $diff_values = comment_driven_inspect_diff_values($old_elem[$key], $new_elem[$key]);
      if (!$diff_values) continue;
      
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
    // now lets check $new_children_keys that might not be present int $old_children_keys
    $non_existent_elem = array('#default_value' => NULL);
    foreach ($new_children_keys as $key) {
      if (in_array($key, $old_children_keys)) {
        continue; // already considered in the above foreach
      }
      // we have no $old_elem[$key] at all
      // just recursing to preserve keys structure
      $diff_values = comment_driven_inspect_diff_values($non_existent_elem, $new_elem[$key]);
      if (!$diff_values) continue;
      
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
    return array($olds, $news);
  }
  
  $old_default = $old_elem['#default_value'];
  $new_default = $new_elem['#default_value'];
  if (empty($old_default)) {
    if (empty($new_default)) {
      return FALSE; // both are empty
    }
    // $old_default is empty or wasn't found,
    // but $new_default isn't empty, therefore it isn't NULL
    // ensure they match in type
    $old_default = NULL;
    settype($old_default, gettype($new_default));
  }
  elseif (empty($new_default)) {
    // $new_default is empty or wasn't found,
    // but $old_default isn't empty, therefore it isn't NULL
    // ensure they match in type
    $new_default = NULL;
    settype($new_default, gettype($old_default));
  }
  else {
    // neither is empty
    // perform the comparison
    if (is_array($old_default)) {
      // if they are non-keyed it shouldn't be a problem to compare them as being keyed
      // since they are expected to be exactly the same or not
      if (comment_driven_equal_keyed_array_in_depth($old_default, $new_default)) {
        return FALSE; // they are equal
      }
      // @TODO: every call to comment_driven_assertion_failed will be removed after alpha/beta stage 
      if (comment_driven_equal_array_values($old_default, $new_default)) comment_driven_assertion_failed('inspector assumed as different arrays in a keyed comparison when they seem to be equal compared as non-keyed');
    }
    else {
      if (_comment_driven_equal_non_array($old_default, $new_default)) {
        return FALSE; // they are equal
      }
    }
  }
  // if they were equal we continued to next iteration
  // therefore, reaching this point means they differ 
  return array($old_default, $new_default);
}

function comment_driven_inspect_deep_clone($var) {
  // seek in depth for objects that came from static cache
  // and should not be shared
  if (is_object($var)) {
    $clone = (object)(array)$var; // cloning compatible with PHP4
    // now seek in depth for possible shared objects
    foreach ((array)$var as $key => $value) {
      $clone->$key = comment_driven_inspect_deep_clone($value);
    }
  }
  elseif (is_array($var)) {
    $clone = $var; // copy
    // now seek in depth for possible shared objects
    foreach ($var as $key => $value) {
      $clone[$key] = comment_driven_inspect_deep_clone($value);
    }
  }
  else {
    // simple type => nothing to seek in depth
    $clone = $var;
  }
  return $clone;
}
