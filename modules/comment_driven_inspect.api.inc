<?php
// $Id$

comment_driven_equality_load();

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
    $errors[] = t('Comparing a revision to ITSELF.');
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
  //comment_driven_access_control($ned_form, $driven_props);
  
  return $ned_form;
}

function comment_driven_inspect_diff_nodes($old_node, $new_node, $driven_props) {
  // pretty much mimic comment_driven_diff_node_state
  // but for two forms instead of a $form vs $form_values
  // also, this version won't take into account any access control
  
  $old_node_form = comment_driven_inspect_build_node_form($old_node, $driven_props); 
  $new_node_form = comment_driven_inspect_build_node_form($new_node, $driven_props);

  // ignoring #access flags
  // Access Control Policies were NOT applied in comment_driven_inspect_build_node_form
  
  $changes = array();
  $property_paths = array_keys($driven_props);
  // both forms must be of the same content type
  // but one of them might have children the other one doesn't have
  // therefore it is required to navigate synchronized,
  // but taking into account they might mismatch
  // by one of them (or both) missing an element
  // since it is possible that the content type definition changed in time
  // and requested nodes belong to a time when this path didn't existed
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
    $meta = $prop['meta']; // until now only needed for wrapper_joint
    $diff_values = comment_driven_inspect_elem_diff_values($old_parent, $new_parent, $meta);
    if ($diff_values) {
      list($old, $new) = $diff_values;
      $label = empty($prop['label']) ? $ancestor_title : $prop['label'];
      $changes[$property_path] = compact('label', 'old', 'new');
    }
  }
  return $changes;
}

function comment_driven_inspect_elem_diff_values($old_elem, $new_elem, $meta) {
  // ignoring #access flags
  // Access Control Policies were NOT applied in comment_driven_inspect_build_node_form
  
  // wrapper detection
  // if values are not at this level pass through it
  // careful, isset is not enough, since it can be NULL (e.g. number)
  // not using in_array, since it compares and matches zero with string
  if (!comment_driven_has_default_value_key($old_elem) && !comment_driven_has_default_value_key($new_elem)) {
    if (!count(element_children($old_elem)) && !count(element_children($new_elem))) {
      // having no children is a dead end (e.g. add_more)
      return FALSE; // ignore it
    }
    
    $old_children_keys = element_children($old_elem); 
    $new_children_keys = element_children($new_elem); 
    // dummy elem to check children_keys that might not be present both sides
    $non_existent_elem = array('#default_value' => NULL);
    
    // while being a wrapper, it can hold expanded radios (e.g. format is a fieldset)
    // if so, each of its children is expected to be radio (or wrapper, e.g. format tips)  
    foreach ($old_children_keys as $key) {
      if ($old_elem[$key]['#type'] === 'radio') {
        // then either of its children will suffice to pick up the #default_value
        // and we have one at hand already
        return comment_driven_inspect_elem_diff_values($old_elem[$key], $new_elem[$key], $meta);
      }
    }
    // check $new_children_keys that might not be present int $old_children_keys
    foreach ($new_children_keys as $key) {
      if ($new_elem[$key]['#type'] === 'radio') {
        // then either of its children will suffice to pick up the #default_value
        // and we have one at hand already
        return comment_driven_inspect_elem_diff_values($non_existent_elem, $new_elem[$key], $meta);
      }
    }
    
    $equal_keys = array();
    foreach ($old_children_keys as $key) {
      $other_elem = empty($new_elem[$key]) ? $non_existent_elem : $new_elem[$key];
      $diff_values = comment_driven_inspect_elem_diff_values($old_elem[$key], $other_elem, $meta);
      if (!$diff_values) {
        // keep track
        // since later isset might not be enough if $old_elem[$key]=NULL 
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
    // check $new_children_keys that might not be present int $old_children_keys
    foreach ($new_children_keys as $key) {
      if (in_array($key, $old_children_keys)) {
        continue; // already considered in the above foreach
      }
      // we have no $old_elem[$key] at all
      // just recursing to preserve keys structure
      $diff_values = comment_driven_inspect_elem_diff_values($non_existent_elem, $new_elem[$key], $meta);
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
    // IMPORTANT: wrapper_joint is in depth (recursive)
    if ($meta['wrapper_joint']) {
      // property requested to include non changed children
      // if at least one of them changed
      // since all of them will be needed at diff_render time
      // IMPORTANT: every children has to have known_default_value
      // (i.e. $element['#default_value'] or a child of #type=radio)
      // otherwise they will be ignored (e.g. format tips)
      foreach ($equal_keys as $key) {
        // if a potential wrapper is found it will be bypassed (e.g. format tips)
        if (comment_driven_has_default_value_key($old_elem[$key])) { 
          $news[$key] = $olds[$key] = comment_driven_known_default_value($old_elem[$key]);
        }
      }
    }
    return array($olds, $news);
  }
  // hitting a non-wrapper beneath the property path
  
  $old_default = comment_driven_known_default_value($old_elem);
  $new_default = comment_driven_known_default_value($new_elem);
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
      $certainly_keyed = FALSE; 
      foreach (array_values($old_default) + array_values($new_default) as $value) { 
        if (is_array($value)) {
          $certainly_keyed = TRUE;
          break;
        }
      }
      if (!$certainly_keyed && comment_driven_equal_array_values($old_default, $new_default)) comment_driven_assertion_failed('inspector assumed as different arrays in a keyed comparison when they seem to be equal compared as non-keyed');
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
