<?php

/*******************************************************************************
 * FORM ALTER
 ******************************************************************************/
/**
 * Implementation of hook_form_alter
 * 
 * Changes the form so that it takes place over multiple pages, adding the
 * necesary fields to do that.
 */
function _autotag_form_alter(&$form, &$form_state, $form_id){
  // If this is not a node form, check if it is the taxonomy vocabulary form.
  // If so, add stuff to it.
  if($form['#id'] != 'node-form'){
    // check if this is the vocabulary edit form, if so, we want to add a 
    // checkbox to give users the option to ignore autotag
    if($form['#id'] == 'taxonomy-form-vocabulary' && $form['module']['#value']=='taxonomy'){
      // Here we can add the extra field
      $disable_vids = variable_get('autotag_disable_vids', array());
      $form['settings']['disableautotag'] = array(
        '#type' => 'checkbox',
        '#title' => t('Disable Autotag'),
        '#default_value' => isset($disable_vids[$form['vid']['#value']]) ? $disable_vids[$form['vid']['#value']] : 0,  
        '#description' => t('If checked, this Vocabulary will not be used by the Autotagger (Vocabularies that are marked as "Multiple Select" and are NOT marked as "Tags" or "Required" will use autotagger)')
      );
      // Finally we need a submit function
      $form['#submit'][] = 'autotag_submit_vocabulary_form';
    } else if ($form_id == 'node_configure'){
      $form['node_preview']['#disabled'] = TRUE;
      $form['node_preview']['#default_value'] = 0;
      $form['node_preview']['#description'] .= ' <span class="error">' . t('This option has been disabled due to a conflict with the Autotag module').'</span>';
    }
    return;
  }
  if(variable_get('autotag_without_user_input', 0)){
    return;
  }
  // If we're here, then this is a node form
  // If no vocabs are relevant to this node, simply return
  $hide_vids = _autotag_get_vids_for_type($form['type']['#value']);
  if(!count($hide_vids)){return;}
  
  // Also check for if we're being called by the node_import module, as if we
  // are, we need to return
  if(strpos($_GET['q'], 'admin/content/node_import') !== FALSE){ return; }
  
  // Autotag doesn't work with the following content types, return if this node
  // is one of these.
  $node_types_to_ignore = variable_get('autotag_broken_node_types', array('webform', 'biblio', 'image'));
  if(in_array($form['type']['#value'], $node_types_to_ignore)){
    if(in_array($form['type']['#value'], variable_get('autotag_totally_borked', array()))){
      return;
    }
    drupal_set_message(t('The term selection interface provided by the Autotag module does not work with the <b>@type</b> content type. Nodes will still be automatically tagged, but without the option to remove unwanted tags.', array('@type' => $form['type']['#value'])));
    return;
  }
  
  // We're here, the form needs some changes, so lets get to it.
  
  // If the Delete button was clicked, we need to send the user to the delete page
  if(isset($form_state['clicked_button']['#id']) && $form_state['clicked_button']['#id'] == 'edit-delete'){
    drupal_goto('node/'.$form['#node']->nid.'/delete');
    return;
  }
  
  // If the form has NOT been submitted, we change the submit button name to
  // next, and remove submit functions from the form
  
  // Firstly, we check to see if the oldtaxonomy is in storage, if not, we put
  // it there.
  if(!isset($form_state['storage']['oldtaxonomy'])){
    $form_state['storage']['oldtaxonomy'] = $form['taxonomy'];
  }
  // Lets make sure the form is valid, we can do this by checking if there are
  // any error messages - FIXME - NOT A GOOD WAY OF DOING THIS, BUT I CAN'T
  // CURRENTLY SEE AN ALTERNATIVE
  // <rant>Why can't form state store the valid status of a form?</rant>
  $form_is_valid = !count(drupal_get_messages('error',FALSE));
  if((!$form_state['submitted'] && !$form_state['post']) || !$form_is_valid){
    // Hide the taxonomy fields -> this can greatly improve the rendering time 
    // for a page with a large classification on it.  We do this on all pages
    if(!$form_is_valid){
      // The form isn't valid, we need to restore the values that the user had
      // entered to the form.
      foreach($form_state['values'] as $key => $value){
        // We need to walk the $form variable for each key, and set the 
        // #default_value to be $value.
        $form = autotag_restore_form_values($form, array('key' => $key, 'value' => $value));
      }
    }
    foreach($form['taxonomy'] as $key=>$value){
      if(in_array($key,$hide_vids)){
        unset($form['taxonomy'][$key]);
      }
    }
    // Hide the taxonomy fieldset if there is nothing left in it.
    $taxonomy_keys = array_keys($form['taxonomy']);
    $unset = true;
    foreach($taxonomy_keys as $key){
      if(substr($key, 0, 1)!='#'){
        $unset = false;
      }
    }
    if($unset){
      // Leave it hidden (not sure why), rather than unset-ing
      $form['taxonomy']['#type'] = 'hidden';
    }
    
    // Store the stubmit for use later
    $form_state['storage']['formbuttons'] = $form['buttons'];
    $form_state['storage']['formsubmit'] = $form['#submit']; 
    
    // Change the button so that it says "Next"
    $form['buttons']['submit']['#value'] = t('Next');
    // Change what will happen when we submit the form (remove the other 
    // submits)
    $form['buttons']['submit']['#submit'] = array('autotag_form_submit');
    $form['#submit'] = array();
  } else {
    // Else we likely have either a submitted form, or one with post shit
    if(count($form_state['storage']['formsubmitted'])){
      // Restore the values to the form from t' previous page.
      $form = $form_state['storage']['formsubmitted'];
      // We're going to have to look for nodereference fields here, specifically
      // the autocomplete variety.  I suspect this is a bug with the 
      // nodereference module, and not autotag, but I'm not certain where the
      // bug actually is.  Surely CCK/Nodereference should be setting the #value
      // part of the field array!
      foreach($form['#field_info'] as $field => $field_details){
        if($field_details['widget']['type'] == 'nodereference_autocomplete'){
          //print_r($field);exit;
          // We've got a node reference, lets fix it dude!
          // Check first if the field is a root field, if not, its part of a
          // group, and we'll need to hunt it down!
          if(isset($form[$field])){
            $form[$field][0]['#value'] = $form[$field][0]['#post'][$field][0]['nid'];    
          } else {
            foreach($form as $group => $group_array){
              if(is_array($group_array) && isset($group_array[$field]) && $group_array['#type'] == 'fieldset'){
                // FOUND IT!
                $form[$group][$field][0]['#value'] = $form[$group][$field][0]['#post'][$field][0]['nid'];
                break;                
              }
            }
          } 
        }
      }
      unset($form_state['storage']['formsubmitted']);
      $form['buttons'] = $form_state['storage']['formbuttons'];
      $form['#submit'] = $form_state['storage']['formsubmit'];
    }
    if(isset($form['buttons'])){
      // Our submit needs to be executed first - before node submit
      $form['buttons']['submit']['#submit'] = array_merge(array('autotag_form_submit'),$form['buttons']['submit']['#submit']);
    }
    // Remove 'upload_node_form_submit'
    $key = array_search('upload_node_form_submit', $form['#submit']);
    if($key){
      unset($form['#submit'][$key]);
    }
    $key = array_search('upload_node_form_submit', $form['buttons']['submit']['#submit']);
    if($key){
      unset($form['buttons']['submit']['#submit'][$key]);
    }
    
    // Add the CSS and JS necesary to style the form
    jquery_ui_add(array('ui.draggable', 'ui.droppable'),'none');
    drupal_add_css(drupal_get_path('module','autotag').'/autotag.css');
    drupal_add_js(drupal_get_path('module','autotag').'/autotag.js');

    // Add a javascript function to update the hidden field with the contents of
    // the "Terms to ignore" div
    $form['buttons']['submit']['#attributes']['onclick'] = 'autotag_update_form();';

    // Search the text of the form, and update the taxonomy accordingly
    if(isset($form_state['values'])){
      $tids = _autotag_search_post($form_state['values']);
      // Also put the old terms in there
      if(is_array($form_state['storage']['oldtaxonomy'])){
        foreach($form_state['storage']['oldtaxonomy'] as $vid => $form_array){
          if(is_array($form_array['#default_value'])){
            foreach($form_array['#default_value'] as $tid){
              $tids[] = array('tid' => $tid, 'vid' => $vid);
            }
          }
        }
      }      
      // Add the tids to the form['#post'] dodah
      foreach($tids as $tid){
        // Check if we already have this vocab in the post
        if(isset($form['#post']['taxonomy'][$tid['vid']])){
          // Append on to the end of the array
          if(is_array($form['#post']['taxonomy'][$tid['vid']])){
            $form['#post']['taxonomy'][$tid['vid']][] = $tid['tid'];
          } else {
            $form['#post']['taxonomy'][$tid['vid']] = array($form['#post']['taxonomy'][$tid['vid']] , $tid['tid']);
          }
        } else {
          // Not set, create it
          $form['#post']['taxonomy'][$tid['vid']] = array($tid['tid']);
        }
        // Also put stuff into form_state TODO - Is this needed?
        if(isset($form_state['values']['taxonomy'][$tid['vid']])){
          // Append on to the end of the array
          if(is_array($form_state['values']['taxonomy'][$tid['vid']])){
            $form_state['values']['taxonomy'][$tid['vid']][] = $tid['tid'];            
          } else {
            $form_state['values']['taxonomy'][$tid['vid']] = array($form_state['values']['taxonomy'][$tid['vid']], $tid['tid']);            
          }
        } else {
          // Not set, create it
          $form_state['values']['taxonomy'][$tid['vid']] = array($tid['tid']);
        }
      }
    }
    // Keep stuff as it is, just add the autotag bollocks to the top      
    $form['autotag']['#type'] = 'fieldset';
    $form['autotag']['#title'] = t('Discovered tags');
    $form['autotag']['#collapsed'] = true;
    $form['autotag']['#collapsible'] = false;
    $form['autotag']['#weight'] = -100;
    $form['autotag']['description'] = array(
      '#value' => '<p class="messages autotag-whack-in-face">'.t('Review the terms below, and then "Save"').'</p><p class="description">'.t('The terms in the "Terms to tag" box were found in your text. Drag \'n\' drop the terms from the "Terms to tag" box to the "Terms to ignore" box if you don\'t want this page/node to be tagged with that term') .'</p>',
      '#weight' => -1000
    );
    $form['autotag']['ignoreterms'] = array(
      '#type' => 'hidden'
    );
    // Build the ignore and tag lists
    $lists = '';
    $ignore_list = '';
    $vocabularies = taxonomy_get_vocabularies();      
    if(isset($form['#node']->nid)){
      $ignore_terms = _autotag_get_ignore_terms($form['#node']->nid);
    } else {
      $ignore_terms = array();
    }
    $add_terms = array();
    if(is_array($form['#post']['taxonomy'])){
      foreach($form['#post']['taxonomy'] as $key => $terms){
        if(in_array($key, $hide_vids)){
          $place_holders = array();
          if(is_array($terms)){
            foreach($terms as $doesntmatter){
              $place_holders[] = '%d';
            }
          } else {
            $place_holders[] = '%d';
            $terms = array($terms);
          }
          if(count($place_holders)){
            $results = db_query("SELECT * FROM {term_data} WHERE tid IN (".implode(',',$place_holders).") ORDER BY name", $terms);
            while($term=db_fetch_object($results)){
              $add_terms[] = $term;
              if(!in_array($term,$ignore_terms)){
                $lists .= '<li class="autotag-dragable" tid="'.$term->tid.'">'.check_plain($term->name).' ('.check_plain($vocabularies[$term->vid]->name).')</li>';
              }
            }
          }
        }
      }
    }  
    foreach($ignore_terms as $term){
      if(in_array($term, $add_terms)){
        $ignore_list .= '<li class="autotag-dragable" tid="'.$term->tid.'">'.check_plain($term->name).' ('.check_plain($vocabularies[$term->vid]->name).')</li>';
      }
    }
    // HTML shit for drag'n'droping
    $form['autotag']['dragndropfields'] = array(
      '#value' => '<div class="autotag-dnd" id="autotag-container">
  <div id="autotag-dnd-left">
    <p id="autotag_remove_all">'.t('Remove all').'</p>
    <h3>'.t('Terms to use').'</h3>
    <ul>
      '.$lists.'
    </ul>
  </div>
  <div id="autotag-dnd-right">
    <h3>'.t('Terms to ignore').'</h3>
    <ul>
      '.$ignore_list.'
    </ul>
  </div> 
</div>');
      
    // Add an additional autocomplete field for adding any additional tags to
    // the classification
    $vocabs = _autotag_get_vids_for_type($form['type']['#value'], FALSE);
    $vocabs_string = '';
    $concater = '';
    foreach($vocabs as $vocab){
      $vocabs_string .= $concater.' '.check_plain($vocab->name);
      $concater = ';';
    }
    $form['autotag']['additionaltags'] = array(
      '#type' => 'textfield',
      '#title' => t('Additional terms to tag this node with'),
      '#description' => t('Terms can be selected from (multiple terms seperated by a comma):').$vocabs_string,
      '#autocomplete_path' => 'autotag/autocomplete/'.$form['type']['#value']
    );
  }
  
  // Add a hidden field for recording the location of ignore and add terms in
  // the drag'n'drop interface.
  $form['autotag']['hideterms'] = array(
    '#type' => 'hidden',
    '#default_value' => '|'
  );
  // Additional submit button higher up the page so that lazy f*&kers don't have to 
  // scroll down.  
  if(isset($form['autotag']['dragndropfields'])){
    $form['autotag']['submit'] = $form['buttons']['submit'];
    $form['autotag']['submit']['#attributes']['class'] = 'autotag-for-the-blind';
    $form['autotag']['submit']['#weight'] = 1000;
  }
  
  // Unset the preview button, it's trouble.
  unset($form['buttons']['preview']);
}

/**
 * Look through the form for some shit!
 */
function autotag_restore_form_values($form, $value){
  foreach($form as $key => $form_part){
    if(is_array($form_part)){
      if($key == $value['key'] && ((isset($form_part['#type']) && $form_part['#type'] != 'fieldset') || is_array($value['value']))){
        if(is_array($value['value'])){
          _autotag_form_set_from_array($form_part, $value['value']);
        } else {
          $form[$key]['#default_value'] = $value['value'];
        }
      } else {
        $form[$key] = autotag_restore_form_values($form_part, $value);
      }
    }
  }
  return $form;
}

// Recursive function called by the above function.
function _autotag_form_set_from_array(&$form, $form_state){
  foreach($form_state as $key => $value){
    if(isset($form[$key]) && is_array($form[$key])){
      if(is_array($value)  && !isset($form[$key]['#default_value'])){
        _autotag_form_set_from_array($form[$key], $value);
      } else {
        $form[$key]['#default_value'] = $value;
      }
    }
  }
}