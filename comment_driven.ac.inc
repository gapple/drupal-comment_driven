<?php
// $Id$

// deferred implementation of comment_driven_access_control
function _comment_driven_access_control(&$ned_form, $driven_props, $full_access = FALSE) {
  // $full_access will grant all roles, but just for those who reached this point
  
  $property_paths = array_keys($driven_props);
  $tree = comment_driven_build_property_paths_tree($property_paths);
  
  // access_mask MUST be the first thing to do
  // to avoid granting access to unrelated elements
  // therefore it is the only responsible for including comment_driven.ac.inc
  comment_driven_access_mask($ned_form, $tree);
  
  if (!$full_access && module_exists('comment_driven_acp')) {
    // apply Access Crontrol Policies
    comment_driven_acp($ned_form, $tree);
  }
  else {
    // grant all roles, but just for those who reached this point
    // because they had drive/moderate permission por this content type
    comment_driven_access_grant_all_roles($ned_form, $tree);
  }
  
  // seal parents which no child left granted
  comment_driven_access_cleanup($ned_form, $tree);
  
  // if there is nothing allowed left,
  // make the caller aware to avoid working in vain
  // the way to know it is if cleanup revoked access to the top level
  // (note that top level access is NEVER granted by any means) 
  return (!isset($ned_form['#access']) || $ned_form['#access']);
}

function comment_driven_access_cleanup(&$ned_form, $tree) {
  // Access Control Policies were already applied
  // seek for elements included in property paths which got all forbidden
  // i.e. if it is an ancestor of driven properties,
  // but none of them remain allowed/accessible
  $children_keys = element_children($tree);
  foreach ($children_keys as $key) {
    // first go in depth (bottom-up)
    comment_driven_access_cleanup($ned_form[$key], $tree[$key]);
  }

  // now check whether it still has at least an allowed child
  foreach (element_children($ned_form) as $key) {
    if (!isset($ned_form[$key]['#access']) || $ned_form[$key]['#access']) {
      // found an accesible child
      return; // it doesn't requires cleanup
    }
  }
  
  // didn't find any accessible child
  // therefore it does requires cleanup
  // even if it is the top level (the whole form itself)
  $ned_form['#access'] = FALSE;
}

function comment_driven_access_mask(&$ned_form, $tree) {
  // forbid access to anything not included in at least a property path
  // i.e. if it is an ancestor of at least a driven property
  // then don't mess with its #access (i.e. don't even touch it)
  
  $children_keys = element_children($tree);
  $has_children = !empty($children_keys);
  if (!$has_children) {
    // found a leaf
    // therefore we are at a property path level
    // don't touch this level neither beneath it
    return;
  }
  
  foreach (element_children($ned_form) as $key) {
    if (in_array($key, $children_keys)) {
      // it is in a property path, seek beneath it
      comment_driven_access_mask($ned_form[$key], $tree[$key]);
      // don't touch this level
      continue;
    }
    // seal it
    $ned_form[$key]['#access'] = FALSE;
    // be paranoiac (in case of bugs in FAPI or custom element types),
    // seal everything beneath it as well
    _comment_driven_access_in_depth($ned_form[$key], FALSE);
  }
}

function comment_driven_access_grant_all_roles(&$ned_form, $tree) {
  // (note that top level access is NEVER granted by any means) 
  $in_depth = variable_get('comment_driven:grant_in_depth', 0);
  // grant access to every property and its ancestors (otherwise it wouldn't be reachable)
  // if in_depth then also enforce access to every element beneath it
  $children_keys = element_children($tree);
  foreach ($children_keys as $key) {
    $ned_form[$key]['#access'] = TRUE;
    comment_driven_access_grant_all_roles($ned_form[$key], $tree[$key]);
  }
  
  // using very explicit variable names,
  // since this is a sensible matter
  $is_top_level = empty($tree['#path']);
  $has_children = !empty($children_keys);
  
  // (note that top level access is NEVER granted by any means) 
  if ($is_top_level || $has_children) {
    // not yet at property path level
    return; // don't fall through leaf treatment (bellow)
  }
  
  // found a leaf
  // therefore we are at a property path level
  if ($in_depth) {
    _comment_driven_access_in_depth($ned_form, TRUE);
  }
}

function _comment_driven_access_in_depth(&$element, $allowed) {
  _comment_driven_prop_in_depth($element, '#access', $allowed);
}

function _comment_driven_prop_in_depth(&$element, $key, $value) {
  foreach (element_children($element) as $child) {
    _comment_driven_prop_in_depth($element[$child], $key, $value);
    $element[$child][$key] = $value;
  }
}

function comment_driven_build_property_paths_tree($property_paths) {
  // IMPORTANT: top level won't have associated #path
  // to recognize it with empty($tree['#path'])
  $tree = array();
  foreach ($property_paths as $property_path) {
    $path_parts = explode(':', $property_path);
    $parent = &$tree;
    foreach ($path_parts as $key) {
      $parent[$key] = array(
        '#path' => empty($parent['#path']) ? $key : ($parent['#path'] . ':' . $key),
      );
      $parent = &$parent[$key];
    }
  }
  return $tree;
}
