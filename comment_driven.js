// $Id$

Drupal.comment_driven = Drupal.comment_driven || {};

Drupal.comment_driven.node_wrap = function() {
  // the comment_form is within id=comment_driven
  // everything else belongs to node_form
  
  // #weight was already provided to make
  // id=comment_driven ligther and id=driven_node_container heavier
  // but we wan't to reorder them both within the form
  // therefore, we'll bring out the the two major pieces (comment_form and empty fieldset)
  // to be able to select everything else to go into id=driven_node_container
  //
  // note that the second one added will be preppended on top
  if (Drupal.settings.comment_driven.node_form_bellow) {
    $('#driven_node_container').prependTo('#comment-form');
    $('#comment_driven').prependTo('#comment-form');
  }
  else {
    $('#comment_driven').prependTo('#comment-form');
    $('#driven_node_container').prependTo('#comment-form');
  }
  
  // bring every node_form element into the fieldset
  // don't rely on class=node-form being set by a theme function
  $('#comment-form > div:not(#comment_driven)').prependTo('#driven_node_container .fieldset-wrapper');
  // make the fieldset visible
  $('#driven_node_container').css('display', 'block');
};

// just once (not a behavior)
$(document).ready(Drupal.comment_driven.node_wrap);
