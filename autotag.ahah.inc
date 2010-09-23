<?php

/**
 * Callback for ahah
 */
function autotag_callback($content_type, $nid = FALSE){
  // Here we process the form, and then return only the taxonomy fieldset to get
  // embeded back in to the form (but with shit selected).  We'll also add a 
  // small message, to reassure the user that something has actually happened!
  $tids = autotag_search_form_array($_POST, $content_type);
  // Add every tid found to a session variable. The ones that are not present in
  // the term_node table once the node is saved, will be added to the autotag 
  // table. We'll use the unique form_id to save the data
  if(!isset($_SESSION['autotag_tids'])){
    $_SESSION['autotag_tids'] = array();
  }
  $_SESSION['autotag_tids'][$_POST['form_build_id']] = $tids;
  // Remove the tids that we don't want adding - we can only do this if we have
  // an nid
  if($nid){
    $result = db_query('SELECT a.tid, vid FROM {autotag} a, {term_data} t WHERE nid = %d AND a.tid = t.tid', $nid);
    while($row = db_fetch_array($result)){
      $key = array_search($row, $tids);
      if($key !== FALSE){
        unset($tids[$key]);
      }
    }
  }
  if($nid){
    $node = node_load(array(
      'nid' => $nid
    ));
  }else{
    $node = array(
      'uid' => $user->uid,
      'name' => (isset($user->name) ? $user->name : ''),
      'type' => $type,
      'language' => ''
    );
  }
  // Magic happens here because of the fact that the form data is in the $_POST
  // array, so we get back all the data in the form filled out!
  if(count($tids)){
    $term_names = array();
    foreach($tids as $tid){
      $term = taxonomy_get_term($tid['tid']);
      $term_names[] = $term->name;
      // It's possible that the taxonomy array isn't set in $_POST, so we need
      // to set it first
      if(!isset($_POST['taxonomy'])){
        $_POST['taxonomy'] = array();
      }
      if(!isset($_POST['taxonomy'][$tid['vid']])){
        // We may be using batax here, lets check
        if(function_exists('batax_use_batax') && !batax_use_batax($tid['vid'], $content_type)){
          $_POST['taxonomy'][$tid['vid']] = array();
        }
      }
      if(is_array($_POST['taxonomy'][$tid['vid']])){
        $_POST['taxonomy'][$tid['vid']][] = $tid['tid'];
      }else{
        // I'm guessing here, but it's likely that this is a result of Batax.
        // Perhaps we need some kind of hook function that allows a module that
        // is responsible for a taxonomy field to update that field with an 
        // additional term.  For now though, we'll simply tag it on as we'd expect
        // batax to do.
        $term_text = $term->name . ':' . $term->tid;
        if(strpos($_POST['taxonomy'][$tid['vid']], $term_text) === FALSE){
          $_POST['taxonomy'][$tid['vid']] .= (strlen(trim($_POST['taxonomy'][$tid['vid']])) ? ', ' : '') . $term_text;
        }
      }
    }
    $message = t('Autotag found the following terms in this node:') . ' ' . check_plain(implode(", ", $term_names));
    if(strlen($message) > 500){
      $message = substr($message, 0, 500) . "... (too many to list)";
    }
  }else{
    $message = t('Autotag found no terms in this node');
  }
  print drupal_to_js(array(
    'status' => TRUE,
    'data' => drupal_get_form($content_type . '_node_form', $node),
    'action' => 'autotag_uncheckbox(response.message)',
    'message' => $message
  ));
  // Clear all error messages.
  drupal_get_messages();
  exit();
}

/**
 * Recursive function which searches a form array, and returns an array of tids
 * that have been found within the form.
 */
function autotag_search_form_array($form_parts, $content_type){
  $tids = array();
  foreach($form_parts as $part){
    if(is_object($part)){
      $part = (array)$part;
    }
    if(is_array($part)){
      $tids = array_merge($tids, autotag_search_form_array($part, $content_type));
    }else{
      // Here we do the searching, so we'll actually use an external method to
      // check the content!
      $tids = array_merge($tids, autotag_search_form_part($part, $content_type));
    }
  }
  return $tids;
}

/**
 * Search text for terms
 */
function autotag_search_form_part($text, $content_type){
  $terms = array();
  // Only search if there is text
  if(trim($text) == ''){return array();}
  // Get the vids that we're to use.
  $vids = _autotag_get_vids_for_type($content_type);
  /**
   * Discovered that the following only seems to work in PHP 5.2, FARP!
   * Thanks to 
   * http://stackoverflow.com/questions/790596/split-a-text-into-single-words
   * $words_including_small = preg_split('/[\p{P}\s\{\}\[\]\(\)]/', strtolower($text), -1, PREG_SPLIT_NO_EMPTY);
   */
  $words_including_small = preg_split('/[\ `!"£$%^&*()_\-+={\[}\]:;@\'~#<,>.?\/|\\\]/', strtolower($text), -1, PREG_SPLIT_NO_EMPTY);
  // lets remove the shitty small words.
  $words = array();
  $words_placeholder = array();
  foreach($words_including_small as $key => $word){
    if(strlen(trim($word)) > 3){
      $words[$key] = $word;
      $words_placeholder[] = "'%s'";
    }
  }
  // Are we tagging just leaves?
  $tag_only_leaves = variable_get('autotag_only_leaves', FALSE);
  if($tag_only_leaves){
    $tag_only_leaves_sql = ' AND t.tid NOT IN (SELECT parent FROM {term_hierarchy}) ';
  }
  // Because I'm sending words as an array, I've also got to do vids in the same way
  $vids_placeholder = array();
  foreach($vids as $doesntmatter){
    $vids_placeholder[] = "vid = %d";
  }
  if(count($words_placeholder) && count($vids)){
    // To make the following SQL command easier to read, it has been spaced!
    $results = db_query(db_rewrite_sql("
          SELECT 
            t.tid, t.vid 
          FROM 
            {term_data} t
          INNER JOIN          
            {term_lowername} l
          ON 
            t.tid = l.tid 
          WHERE 
            lowername IN (" . implode(",", $words_placeholder) . ") AND 
            (" . implode(" OR ", $vids_placeholder) . ") $tag_only_leaves_sql ", 't', 'tid') . '
          UNION 
          ' . db_rewrite_sql("SELECT 
            t.tid, t.vid 
          FROM 
            {term_data} t
          INNER JOIN          
            {term_synonym} s
          ON
            s.tid = t.tid 
          WHERE             
            LOWER(s.name) IN (" . implode(",", $words_placeholder) . ") AND 
            (" . implode(" OR ", $vids_placeholder) . ") $tag_only_leaves_sql", 't', 'tid'), array_merge($words, $vids, $words, $vids));
    while($row = db_fetch_array($results)){
      $terms[] = array(
        'tid' => $row['tid'],
        'vid' => $row['vid']
      );
    }
    $total_words = count($words);
    $sql_array = array();
    $sql_array_syn = array();
    $words_array = array();
    $words_array_syn = array();
    foreach($words as $key => $word){
      if(isset($words_including_small[$key + 1])){
        // Now lets search for "pairs" and search on "like".  If any hit, check back on the result against
        // the original field
        $sql_array[] = " lowername LIKE '%s%%%s%%' ";
        $words_array[] = $word;
        $words_array[] = $words_including_small[$key + 1];
        $sql_array_syn[] = " LOWER(t.name) LIKE '%s%%%s%%' ";
        $words_array_syn[] = $word;
        $words_array_syn[] = $words_including_small[$key + 1];
      }
    }
    if($tag_only_leaves){
      $tag_only_leaves_sql = ' AND tid NOT IN (SELECT parent FROM {term_hierarchy}) ';
    }
    if(count($sql_array)){
      $results = db_query(db_rewrite_sql("
            SELECT 
              tid
            FROM 
              {term_lowername} t 
            WHERE 
              (" . implode(" OR ", $sql_array) . ") $tag_only_leaves_sql ", 't', 'tid') . '
            UNION
            ' . db_rewrite_sql("SELECT 
              tid 
            FROM 
              {term_synonym} t
            WHERE 
              (" . implode(" OR ", $sql_array_syn) . ") $tag_only_leaves_sql", 't', 'tid'), array_merge($words_array, $words_array_syn));
      while($row = db_fetch_array($results)){
        // Get the full term name, and its synonyms, and check back against the field
        $results_2 = db_query(db_rewrite_sql("
              SELECT 
                tid, vid, LOWER(name) AS name
              FROM 
                {term_data} t
              WHERE 
                tid = %d AND
                (" . implode(" OR ", $vids_placeholder) . ")", 't', 'tid') . '
              UNION
              ' . db_rewrite_sql("SELECT
                s.tid, vid, LOWER(s.name) AS name
              FROM 
                {term_data} t
              INNER JOIN              
                {term_synonym} s
              ON 
                s.tid = %d
              WHERE 
                t.tid=s.tid AND 
                (" . implode(" OR ", $vids_placeholder) . ")", 't', 'tid'), array_merge(array(
          $row['tid']
        ), $vids, array(
          $row['tid']
        ), $vids));
        while($row_2 = db_fetch_array($results_2)){
          if(strpos(strtolower($text), $row_2['name']) !== FALSE){
            $terms[] = array(
              'tid' => $row_2['tid'],
              'vid' => $row_2['vid']
            );
          }
        }
      }
    }
  }
  return $terms;
}