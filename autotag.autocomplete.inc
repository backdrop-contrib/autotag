<?php

function autotag_autocomplete($type = 'page', $string=''){
  module_load_include('functions.inc','autotag');
  $vids = _autotag_get_vids_for_type($type);
  /**
   * The following is taken straight from the taxonomy module, with a small alteration to the SQL
   */
  $regexp = '%(?:^|,\ *)("(?>[^"]*)(?>""[^"]* )*"|(?: [^",]*))%x';
  preg_match_all($regexp, $string, $matches);
  $array = $matches[1];
  
  // Fetch last tag
  $matches = array();
  $last_string = trim(array_pop($array));
  if ($last_string != '') {
    $result = db_query_range(db_rewrite_sql("SELECT t.tid, t.name FROM {term_data} t WHERE t.vid IN (%s) AND LOWER(t.name) LIKE LOWER('%s%%')", 't', 'tid'), implode(",",$vids), $last_string, 0, 10);
  
    $prefix = count($array) ? implode(', ', $array) .', ' : '';
  
    while ($tag = db_fetch_object($result)) {
      $n = $tag->name;
      // Commas and quotes in terms are special cases, so encode 'em.
      if (strpos($tag->name, ',') !== FALSE || strpos($tag->name, '"') !== FALSE) {
        $n = '"'. str_replace('"', '""', $tag->name) .'"';
      }
      $matches[$prefix . $n] = check_plain($tag->name);
    }
  }
  print drupal_to_js($matches);
  exit();
}