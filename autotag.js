/*
 * Define a function that is called when the ahah action is called 
 * (beforeSubmit).  This funciton will update the values for the WYSIWYG form
 * fields, which is done on a normal submit.
 */
function autotag_submitextra(form_values, element, options){
  $.each(Drupal.wysiwyg.instances, function(index, value){
    if(value.status != undefined && value.status == 1){
      var editor = tinyMCE.get(value.field);
      editor.save();
      $.each(form_values, function(i, m){
        if(m.name == $('#'+value.field).attr('name')){
          form_values[i]['value'] = $('#'+value.field).val();
        }
      });
    }
  });
}
/*
 * Uncheck the autotag check box
 */
function autotag_uncheckbox(message){
  $('#edit-taxonomyautotagcheckbox').attr('checked', '');
  $('#autotag-message').html(message);
  $('#autotag-message').slideDown();
}