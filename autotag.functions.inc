<?php

/**
 * Takes a field and term and checks for a match.  Field can be an array, or
 * a single string.  If an array, it recurses till it gets a string
 * 
 * Search algorithm:
 * 
 * Split field into words
 * foreach word
 *   does the word match any terms (relevant to this node type)
 *     yes:add the term id to the list
 *     no:continue
 * foreach word
 *   does the word combined with the word after it look like any terms (...)
 *     get the full term (which could be more than two words)
 *     does the full term match anywhere in the field
 *       yes:add the term id to the list
 *       no:continue
 */
function _autotag_search_field($field, $vids){
  $terms=array();
  if(is_array($field) || is_object($field)){
    // Field is an array (and likely to be an array of arrays).  Lets
    // recurse into it and check away
    foreach($field as $field_part=>$value){
      $terms = array_merge($terms,_autotag_search_field($value, $vids));
    }
  } else {
    // Only search if there is text
    if(trim($field)==''){return array();}
    // Field is raw text search it for stuff
    /**
     * Discovered that the following only seems to work in PHP 5.2, FARP!
    // Thanks to http://stackoverflow.com/questions/790596/split-a-text-into-single-words
    $words_including_small = preg_split('/[\p{P}\s\{\}\[\]\(\)]/', strtolower($field), -1, PREG_SPLIT_NO_EMPTY);
     */
    
    $words_including_small = preg_split('/[\ `!"£$%^&*()_\-+={\[}\]:;@\'~#<,>.?\/|\\\]/', strtolower($field), -1, PREG_SPLIT_NO_EMPTY);
    // lets remove the shitty small words.
    $words = array();
    $words_placeholder = array();
    foreach($words_including_small as $key => $word){
      if(strlen(trim($word))>3){
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
      $results = db_query("
          SELECT 
            t.tid, t.vid 
          FROM 
            {term_lowername} l,
            {term_data} t 
          WHERE 
            t.tid = l.tid AND 
            lowername IN (".implode(",",$words_placeholder).") AND 
            (".implode(" OR ",$vids_placeholder).") $tag_only_leaves_sql 
          UNION 
          SELECT 
            t.tid, t.vid 
          FROM 
            {term_data} t, 
            {term_synonym} s 
          WHERE 
            s.tid = t.tid AND 
            LOWER(s.name) IN (".implode(",",$words_placeholder).") AND 
            (".implode(" OR ",$vids_placeholder).") $tag_only_leaves_sql",
        array_merge($words,$vids,$words,$vids));
        
      while($row=db_fetch_array($results)){
        $terms[] = array('tid'=>$row['tid'], 'vid'=>$row['vid']);
      }
      $total_words = count($words);
      $sql_array = array();
      $sql_array_syn = array();
      $words_array = array();
      $words_array_syn = array();
      foreach($words as $key => $word){
        if(isset($words_including_small[$key+1])){
          // Now lets search for "pairs" and search on "like".  If any hit, check back on the result against
          // the original field
          $sql_array[] = " lowername LIKE '%s%%%s%%' ";
          $words_array[] = $word;
          $words_array[] = $words_including_small[$key+1];
          $sql_array_syn[] = " LOWER(s.name) LIKE '%s%%%s%%' ";
          $words_array_syn[] = $word;
          $words_array_syn[] = $words_including_small[$key+1];
        }
      }
      if($tag_only_leaves){
        $tag_only_leaves_sql = ' AND tid NOT IN (SELECT parent FROM {term_hierarchy}) ';
      }
      if(count($sql_array)){
        $results = db_query("
            SELECT 
              tid
            FROM 
              {term_lowername} l 
            WHERE 
              (".implode(" OR ",$sql_array).") $tag_only_leaves_sql 
            UNION
            SELECT 
              tid 
            FROM 
              {term_synonym} s
            WHERE 
              (".implode(" OR ",$sql_array_syn).") $tag_only_leaves_sql",
          array_merge($words_array, $words_array_syn));
        
        while($row=db_fetch_array($results)){
          // Get the full term name, and its synonyms, and check back against the field
          $results_2 = db_query("
              SELECT 
                tid, vid, LOWER(name) AS name
              FROM 
                {term_data}
              WHERE 
                tid = %d AND
                (".implode(" OR ",$vids_placeholder).")
              UNION
              SELECT
                s.tid, vid, LOWER(s.name) AS name
              FROM 
                {term_synonym} s,
                {term_data} t
              WHERE 
                s.tid = %d AND
                t.tid=s.tid AND 
                (".implode(" OR ",$vids_placeholder).")",
            array_merge(array($row['tid']),$vids,array($row['tid']),$vids));
            
          while($row_2=db_fetch_array($results_2)){
            if(strpos(strtolower($field),$row_2['name'])!== FALSE){
              $terms[] = array('tid'=>$row_2['tid'],'vid'=>$row_2['vid']);
            }
          }
        }
      }
    }
  }
  return $terms;
}


/**
 * Following function takes the posted shit in a form, and searches it. EASY!
 */
function _autotag_search_post($values){
  $vids = _autotag_get_vids_for_type($values['type']);
  $fields = _autotag_fields_to_search_post($values, $values['type']);
  $terms_to_return = array();
  foreach($fields as $field){
    // Lets split the fields up by spaces, and search on each word in the term_data table.
    if(isset($values[$field])){
      $terms = _autotag_search_field($values[$field], $vids);
      if(count($terms)){
        $terms_to_return = array_merge($terms_to_return, $terms);
      }
    }
  }
  return $terms_to_return;
}


/**
 * Implementation of hook_nodeapi
 * 
 * This saves the hidden terms in the database.  This is done here, as the nid
 * may not be known on submit (if the node is new).
 */
function _autotag_nodeapi(&$node, $op, $a3 = NULL, $a4 = NULL){
  // This is completely broken with some node types
  if(in_array($node->type, variable_get('autotag_totally_borked', array()))){
    return;
  }
  switch($op){
    case 'update':
      foreach($_POST['files'] as $fid => $file_details){
        db_query("UPDATE {upload} SET description = '%s', weight = %d, list = %d WHERE fid = %d AND nid = %d AND vid = %d", $file_details['description'], $file_details['weight'], $file_details['list'], $fid, $node->nid, $node->vid);
      }
    case 'insert':
      // Return if this node doesn't have any autotag vids associated with it
      $vids = _autotag_get_vids_for_type($node->type);
      if(!count($vids)){return;}
      
      $all_tids = _autotag_search_node($node);
      $tids_to_ignore = explode("|",$node->hideterms);      
      // Remove the ignore ones from the add ones
      if(!isset($node->taxonomy)){
        $terms = array();
      } else {
        $terms = $node->taxonomy;
      }
      // Check to see whether or not we have objects
      $terms_are_objects = false;
      foreach($terms as $term){
        if(is_object($term)){
          $terms_are_objects = true;
        }
        break;
      }
      foreach($all_tids as $tid){
        if($terms_are_objects){
          $terms[$tid['tid']] = taxonomy_get_term($tid['tid']);
        } else {
          if(!in_array($tid['tid'], $tids_to_ignore)){
            if(is_array($terms[$tid['vid']])){
              if(!isset($terms[$tid['vid']]) || !in_array($tid['tid'], $terms[$tid['vid']])){
                $terms[$tid['vid']][] = $tid['tid'];
              }
            } else {
              // If there isn't currently anything set
              if(is_string($terms[$tid['vid']]) && trim($terms[$tid['vid']]) == ''){
                $terms[$tid['vid']] = $tid['tid'];
              } else {
                // else combine
                $terms[$tid['vid']] = array($tid['tid'], $terms[$tid['vid']]);
              }
            }
          }
        }
      }
      // Remove the ignore ones, from terms already associated with the node
      foreach($tids_to_ignore as $tid){
        if(is_numeric($tid)){
          // Remove this from $node->taxonomy
          foreach($terms as $vocab_key => $vocab_value){
            if(!is_array($vocab_value)){
              if($vocab_value == $tid){
                unset($terms);
              }
            } else {
              foreach($vocab_value as $term_key => $term_value){
                if($term_value == $tid){
                  unset($terms[$vocab_key][$term_key]);
                }
              }
            }
          }
        }
      }
      // We've also got additional tags
      $regexp = '%(?:^|,\ *)("(?>[^"]*)(?>""[^"]* )*"|(?: [^",]*))%x';
      preg_match_all($regexp, $node->additionaltags, $matches);
      if(count($matches[1])){
        $words_place_holders = array();
        foreach($matches[1] as $key => $value){
          $matches[1][$key] = trim($value, '"');
          $words_place_holders[] = "'%s'";
        }
        $result = db_query("SELECT tid, name FROM {term_data} WHERE vid IN (%s) AND LOWER(name) IN (".implode(",",$words_place_holders).")", array_merge(array(implode(",",$vids)), $matches[1]));
        while($row = db_fetch_array($result)){
          if(!isset($terms[$row['vid']]) || !in_array($row['tid'], $terms[$row['vid']])){
            $terms[$row['vid']][] = $row['tid'];
          }
        }      
      }
      if($terms_are_objects){
        $terms = array_unique(array_flatten(array($terms)));
      }
      $terms['tags'] = $node->taxonomy['tags'];
      taxonomy_node_save($node, $terms);
      _autotag_update_table($node->nid, $tids_to_ignore);
      break;
  }
}


function array_flatten($array) {
  $new_array = array();
  foreach ($array as $child) {
    if (is_array($child) || is_object($child)) {
      $new_array = array_merge($new_array, array_flatten($child));
    } else {
      $new_array[] = $child;
    }
  }
  return $new_array;
}

/**
 * Return an array of vids which apply to a specific node type
 */
function _autotag_get_vids_for_type($type, $just_vids = TRUE){
  $vocabularies = taxonomy_get_vocabularies();
  $vids = array();
  $disable_vids = variable_get('autotag_disable_vids', array());
  foreach($vocabularies as $vocabulary){
    if(in_array($type,$vocabulary->nodes) &&
       $vocabulary->required == 0 &&
       $vocabulary->multiple == 1 &&
       $vocabulary->module == 'taxonomy' &&
       (
         !isset($disable_vids[$vocabulary->vid]) ||
         (
           isset($disable_vids[$vocabulary->vid]) &&
           !$disable_vids[$vocabulary->vid]
         )
       )){
      if($just_vids){
        $vids[] = $vocabulary->vid;
      } else {
        $vids[] = $vocabulary;
      }
    }
  }
  return $vids;
}

/**
 * Following function gets the terms from the database, and does the tagging
 * 
 * This takes a node as a parameter, and returns the additional terms that the
 * node should be tagged with.
 */
function _autotag_search_node($node){
  // First get all vocabularies, and see which ones apply to this content type
  $vids = _autotag_get_vids_for_type($node->type);
  // Get all the fields to search from this node  
  $fields = _autotag_fields_to_search($node);
  $terms_to_return = array();
  foreach($fields as $field){
    // Lets split the fields up by spaces, and search on each word in the term_data table.
    if(isset($node->$field)){
      $terms_to_return = array_merge($terms_to_return, _autotag_search_field($node->$field, $vids));
      /*      foreach($terms as $term){
        
        $node->taxonomy[$term['vid']][$term['tid']] = $term['tid'];
      }*/
    }
  }
  return $terms_to_return;
}

/**
 * Function simply returns an array of fields to search when given a node
 */
function _autotag_fields_to_search($node){
  $fields = array();
  $ignore_fields = array(
    'op','attach-url','nid','vid','status','type','created','comment','changed','promote','sticky','revision_timestamp','log','format','uid','name','picture','data','path','last_comment_timestamp','last_comment_name','comment_count','files','iid','taxonomy','readmore','content','links','additionaltags'
  );
  foreach($node as $key=>$value){
    if(!in_array($key, $ignore_fields)){
      $fields[] = $key;
    }
  }
  return $fields;
}
function _autotag_fields_to_search_post($post_form, $type){
  $fields = array();
  $ignore_fields = array(
    'op','attach-url','nid','vid','status','type','created','comment','changed','promote','sticky','revision_timestamp','log','format','uid','name','picture','data','path','last_comment_timestamp','last_comment_name','comment_count','files','iid','taxonomy','readmore','content','links','additionaltags'
  );
  if(is_object($post_form) || is_array($post_form)){
    foreach($post_form as $key=>$value){
      if(!in_array($key, $ignore_fields)){
        $fields[] = $key;
      }
    }
    return $fields;
  }
  return array();
}

/**
 * Inserts ths stuff into the autotag table
 */
function _autotag_update_table($nid, $values){
  // Add the ignore terms to the database so that the next time the node is edited
  // the terms are in the ignore list, and not the add list.
  db_query("DELETE FROM {autotag} WHERE nid=%d",$nid);
  $tids_nids = array();
  $tids_nid_placeholder = array();
  foreach($values as $value){
    if($value!=''){
      $tids_nid_placeholder[] = "(%d,%d)";
      $tids_nids[] = $nid;
      $tids_nids[] = $value;
    }
  }
  if(count($tids_nid_placeholder)){
    db_query("INSERT INTO {autotag} (nid, tid) VALUES ".implode(",",$tids_nid_placeholder), $tids_nids);
  }
}

/**
 * Gets the terms that this node will ignore
 */
function _autotag_get_ignore_terms($nid){
  $results = db_query("SELECT a.tid FROM {autotag} a,{term_data} t WHERE nid = %d AND a.tid = t.tid ORDER BY vid,name", $nid);
  $terms = array();
  while($nid = db_fetch_array($results)){
    $terms[] = taxonomy_get_term($nid['tid']);
  }
  return $terms;
}