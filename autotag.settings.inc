<?php

/**
 * autotag settings page
 */
function autotag_admin_settings(){
  $form['autotag_only_leaves'] = array(
    '#type' => 'checkbox',
    '#title' => t('Tag nodes with only terms at the very tip of a taxonomy (i.e. Terms which are not the parent of any other term)'),
    '#default_value' => variable_get('autotag_only_leaves', false),
    '#description' => t('If checked, only the leaf terms will be used by autotag')
  );
  $form['content_types'] = array(
    '#type' => 'fieldset',
    '#title' => t('Default for "Autotag on save"'),
    '#collapsible' => TRUE
  );
  $form['content_types']['autotag_save_checkbox'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Content types'),
    '#default_value' => variable_get('autotag_save_checkbox', array()),
    '#options' => array_map('check_plain', node_get_types('names')),
    '#description' => t('Set the default status for the "Autotag on save" checkbox on every node form by content type.')
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
    '#submit' => array(
      'autotag_tag_content_type_submit'
    ),
    '#executes_submit_callback' => TRUE
  );
  return system_settings_form($form);
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
      $types = "'" . implode("','", $types) . "'";
      $result = db_query(db_rewrite_sql("SELECT nid FROM {node} n WHERE type IN ($types)"));
      $nodes = array();
      while($nid = db_result($result)){
        $nodes[] = $nid;
      }
      autotag_mass_update($nodes);
    }else{
      drupal_set_message('Please select a content type to autotag', 'error');
    }
  }
}

/**
 * Callback for node_operation to re-autotag nodes.
 */
function autotag_mass_update($nodes){
  // Following ripped off from nodes module.
  // We use batch processing to prevent timeout when updating a large number
  // of nodes.
  if(count($nodes) > 10){
    $batch = array(
      'operations' => array(
        array(
          '_autotag_mass_update_batch_process',
          array(
            $nodes
          )
        )
      ),
      'finished' => '_autotag_mass_update_batch_finished',
      'title' => t('Processing'),
      // We use a single multi-pass operation, so the default
      // 'Remaining x of y operations' message will be confusing here.
      'progress_message' => '',
      'error_message' => t('The update has encountered an error.'),
      'file' => drupal_get_path('module', 'autotag') . '/autotag.settings.inc'
    );
    batch_set($batch);
  }else{
    foreach($nodes as $nid){
      _autotag_mass_update_helper($nid);
    }
    drupal_set_message(t('The update has been performed.'));
  }
}

/**
 * Helper function for above.
 * 
 * This is ugly, but it seems to work.
 */
function _autotag_mass_update_helper($nid){
  $node = node_load(array('nid' => $nid));
  $node->taxonomyautotagcheckbox = 1;
  $post = $_POST;
  $_POST = (array)$node;
  node_save($node);
  $_POST = $post;
}

/**
 * Node Mass Update Batch operation
 */
function _autotag_mass_update_batch_process($nodes, &$context){
  if(!isset($context['sandbox']['progress'])){
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = count($nodes);
    $context['sandbox']['nodes'] = $nodes;
  }
  // Process nodes by groups of 5.
  $count = min(5, count($context['sandbox']['nodes']));
  for($i = 1; $i <= $count; $i++){
    // For each nid, load the node, reset the values, and save it.
    $nid = array_shift($context['sandbox']['nodes']);
    _autotag_mass_update_helper($nid);
    // Update our progress information.
    $context['sandbox']['progress']++;
  }
  // Inform the batch engine that we are not finished,
  // and provide an estimation of the completion level we reached.
  if($context['sandbox']['progress'] != $context['sandbox']['max']){
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
  }
}

/**
 * Node Mass Update Batch 'finished' callback.
 */
function _autotag_mass_update_batch_finished($success, $results, $operations){
  if($success){
    drupal_set_message(t('The update has been performed.'));
  }else{
    drupal_set_message(t('An error occurred and processing did not complete.'), 'error');
  }
}