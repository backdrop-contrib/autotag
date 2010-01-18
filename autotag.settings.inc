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
  return system_settings_form($form);
}