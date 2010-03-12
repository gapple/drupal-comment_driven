<?php
// $Id$

/**
 * Global settings form.
 */
function comment_driven_settings(&$form_state) {
  drupal_set_title(t('Comment driven global settings'));
  
  $form['workflow'] = array(
    '#type' => 'fieldset',
    '#title' => t('Workflow'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  
  $varname = 'comment_driven:enforce_new_revision';
  $form['workflow'][$varname] = array(
    '#type' => 'checkbox',
    '#title' => t('Enforce new revision.'),
    '#description' => t('Even when the content type may have revisions disabled this option will enforce a new revision whenever a driven property change is detected.')
      . ' ' . t('Therefore it only affects those content types with comment driven enabled AND revisions disabled.'),
    '#default_value' => variable_get($varname, 1),
  );
  
  $form['theme_support'] = array(
    '#type' => 'fieldset',
    '#title' => t('Theme support'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  
  $varname = 'comment_driven:fieldset_wrapper_selector';
  $form['theme_support'][$varname] = array(
    '#type' => 'radios',
    '#title' => t('Driven fieldset'),
    '#description' => t('Disabling the fieldset doesn\'t support to position the driven properties above the comment form.'),
    '#default_value' => variable_get($varname, '.fieldset-wrapper'),
    '#options' => array(
      '.fieldset-wrapper' => t('Default'),
      '.fieldset-content' => t('Open Atrium'),
      // I don't like this idea, but it was requested [#722792]
      '' => t('Disabled'),
    ),
  );
  
  $varname = 'comment_driven:node_form_bellow';
  $form['theme_support']['node_form'][$varname] = array(
    '#type' => 'checkbox',
    '#title' => t('Display driven properties bellow comment form.'),
    '#description' => t('Allows to control whether the driven properties appear above or bellow the comment form.'),
    '#default_value' => variable_get($varname, 0),
  );
  
  $form['render'] = array(
    '#type' => 'fieldset',
    '#title' => t('Render'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  
  // lets babysit restless sitebuilders
  // gone property/value might not be discriminable when complex queries are used
  // to know if the missing result was due to a missing property or value
  // would be too much to ask for error reporting
  // if the module knows it should detail it in the log
  // otherwise, be as accurate as possible
  $varname = 'comment_driven:warn_prop_or_val_gone';
  $form['render'][$varname] = array(
    '#type' => 'checkbox',
    '#title' => t('Warn live render gone properties/values.'),
// @TODO: advanced_help
// when warning: displayed messages should be vage, visitor oriented, logs should be detailed
// otherwise just avoid crashing and continue 
///bypasses a property that is no longer supported or displays inaccurate changes due to no longer existing values
// know that live render should be disabled for this content type
// or comment_driven_log table should be moderately cleared

// property gone
// t('(either the property was deleted from the content type or the responsible module got disabled)')
// @TODO: advanced_help
// value gone
/* @TODO: interesting case:
- free tagging
- the node has "tag1, tag2"
- tag1 gets deleted
- comment driven loads the node_form which doesn't have tag1 any more
-- saving without making any change
--- would register tag1 as removed by the comment
--- the diff for the old value won't be achievable
-- saving the node with "tag1, tag2" again (manually set tag1 back)
--- would re-create tag1
--- the diff won't show any difference
- diff won't be achievable if can't find the matching term for either of the sides old/new
- every taxonomy & cck will have ids, not human-readable values
-- (except for free tagging, title/body, and a few others)
*/
    '#description' => t('Warn whenever live render displays inaccurate changes due to missing properties, values or modules responsible of their handling.'),
    '#default_value' => variable_get($varname, 1),
  );
  
  return system_settings_form($form);
}
