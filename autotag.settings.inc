<?php
/**
 * autotag settings page
 */
function autotag_admin_settings() {
  $form['autotag_without_user_input'] = array(
    '#type' => 'checkbox',
    '#title' => t('No user input required before auto tagging.'),
    '#default_value' => variable_get('autotag_without_user_input', false),
    '#description' => t('If checked, no user input will be required to automatically tag a node')
  );
  $form['autotag_only_leaves'] = array(
    '#type' => 'checkbox',
    '#title' => t('Tag nodes with only terms at the very tip of a taxonomy (i.e. Terms which are not the parent of any other term)'),
    '#default_value' => variable_get('autotag_without_user_input', false),
    '#description' => t('If checked, only the leaf terms will be used by autotag')
  );
  $form['content_types'] = array(
    '#type' => 'fieldset',
    '#title' => t('Never autotag Content types'),
    '#collapsible' => TRUE,
  );
  $form['content_types']['autotag_dont_autotag'] = array('#type' => 'checkboxes',
    '#title' => t('Content types'),
    '#default_value' => variable_get('autotag_dont_autotag', array()),
    '#options' => array_map('check_plain', node_get_types('names')),
    '#description' => t('Select content types to NEVER automatically tag.'),
  );
  $form['autotag_by_content_type'] = array(
    '#type' => 'fieldset',
    '#title' => t('Re-Autotag by content type'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE
  );
  $form['autotag_by_content_type']['content_types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Content types'),
    '#description' => t('Note, re-autotagging existing content will not only update the tags associated with the node, but it will also update the date/time the node was last edited, and can also update other settings, like automatically generated aliases.'),
    '#default_value' => array_map('check_plain', node_get_types('names')),
    '#options' => array_map('check_plain', node_get_types('names')),
    '#theme' => 'autotag_admin_settings'
  );
  $form['autotag_by_content_type']['autotag_content_type_submit'] = array(
    '#type' => 'button',
    '#value' => t('Autotag selected content types'),
    '#submit' => array('autotag_tag_content_type_submit'),
    '#executes_submit_callback' => TRUE
  );
  return system_settings_form($form);
}


/**
 * Theme node administration overview.
 *
 * @ingroup themeable
 */
function theme_autotag_admin_settings($element) {
  $headers = array(theme('table_select_header_cell'));
  foreach($element['#options'] as $content_type => $content_type_name){
    $rows[] = array(
      drupal_render($element[$content_type])
    );
  }
  return theme('table', $headers, $rows);
}

function autotag_tag_content_type_submit($form, &$form_state){
  if($form_state['clicked_button']['#value'] == 'Autotag selected content types'){
    // We're not goint to do the necessary shit we need to do here, yay.
    $types = array();
    foreach($form_state['values']['content_types'] as $type){
      if($type){
        $types[] = $type;
      }
    }
    if(count($types)){
      $types = "'".implode("','", $types)."'";
      $result = db_query("SELECT nid FROM node WHERE type IN ($types)");
      $nodes = array();
      while($nid = db_result($result)){
        $nodes[] = $nid;
      }
      autotag_mass_update($nodes);
    } else {
      drupal_set_message('Please select a content type to autotag', 'error');
    }
  }
}