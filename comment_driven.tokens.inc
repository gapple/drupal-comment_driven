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
      $live_render = variable_get('comment_driven:live_render_' . $node_type, 0);
      
      $render_diff = $live_render ? comment_driven_render_diff($log->changes) : $log->render_diff;
      foreach ($render_diff as $keyed_row) {
        $rows[] = array(
          strip_tags($keyed_row['label']) . ':', 
          strip_tags($keyed_row['old']), 
          '&raquo;', 
          strip_tags($keyed_row['new'])
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
