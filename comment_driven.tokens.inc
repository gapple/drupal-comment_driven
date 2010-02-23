<?php
// $Id$

/**
 * Implementation of hook_token_list().
 */
function comment_driven_token_list($type = 'all') {
  if ($type == 'comment' || $type == 'all') {
    $tokens = array();

    $description = t('Diff summary of driven changes made by the comment');
    $tokens['comment'] = array(
      'comment-driven-diff-table' => $description . ' (HTML table)',
      'comment-driven-diff-ulist' => $description . ' (HTML ul/li)',
      'comment-driven-diff-crlf'  => $description . ' (plain text with \\r\\n aka CRLF)',
      'comment-driven-diff-lf'    => $description . ' (plain text with \\n aka LF)',
      'comment-driven-diff-cr'    => $description . ' (plain text with \\r aka CR)',
    );
    
    return $tokens;
  }
}

/**
 * Implementation of hook_token_list().
 */
function comment_driven_token_values($type, $object = NULL) {
  if ($type == 'comment') {
    $comment = (object)$object;
    
    if ($log = comment_driven_load_log($comment->cid)) {
      $node_type = node_get_types('type', node_load($comment->nid))->type;
      $live_render = comment_driven_is_live_render($node_type);
      $diff_render = $live_render ? comment_driven_diff_render($node_type, $log->changes) : $log->diff_render;
      
      for ($render_index = 0; $render_index < count($diff_render); $render_index++) {
        extract($diff_render[$render_index]); // $label, $outgoing, $incoming
        $rows[] = array(
          strip_tags($label) . ': ', 
          strip_tags($outgoing), 
          '&raquo;', 
          strip_tags($incoming),
        );
      }
      $tokens['comment-driven-diff-table'] = theme('comment_driven_simple_table', $rows);
      $tokens['comment-driven-diff-ulist'] = theme('comment_driven_simple_ulist', $rows);
      
      $plain_text = array();
      foreach ($rows as $index => $row) {
        $rows[$index][2] =  '>>'; // replace &raquo;
        $plain_text[] = implode(' ', $rows[$index]); // concatenate
      }
      $tokens['comment-driven-diff-crlf']  = implode("\r\n", $plain_text);
      $tokens['comment-driven-diff-lf']    = implode("\n", $plain_text);
      $tokens['comment-driven-diff-cr']    = implode("\r", $plain_text);
    }
    else {
      $tokens['comment-driven-diff-table'] = '';
      $tokens['comment-driven-diff-ulist'] = '';
      $tokens['comment-driven-diff-crlf']  = '';
      $tokens['comment-driven-diff-lf']    = '';
      $tokens['comment-driven-diff-cr']    = '';
    }
    
    return $tokens;
  }
}
