<?php
$feature  = $variables['node']->feature;  ?>

<div id="tripal_feature-base-box" class="tripal_feature-info-box tripal-info-box">
  <div class="tripal_feature-info-box-title tripal-info-box-title">Details</div>
  <div class="tripal_feature-info-box-desc tripal-info-box-desc"></div> <?php
   
  // the $headers array is an array of fields to use as the colum headers. 
  // additional documentation can be found here 
  // https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7
  // This table for the analysis has a vertical header (down the first column)
  // so we do not provide headers here, but specify them in the $rows array below.
  $headers = array();
  
  // the $rows array contains an array of rows where each row is an array
  // of values for each column of the table in that row.  Additional documentation
  // can be found here:
  // https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7 
  $rows = array();

  // Name row
  $rows[] = array(
    array(
      'data' => 'Name',
      'header' => TRUE
    ),
    $feature->name
  );
  // Unique Name row
  $rows[] = array(
    array(
      'data' => 'Unique Name',
      'header' => TRUE
    ),
    $feature->uniquename
  );
  // Type row
  $rows[] = array(
    array(
      'data' => 'Type',
      'header' => TRUE
    ),
    $feature->type_id->name
  );
  // Organism row
  if ($feature->organism_id->nid) {
    $organism = l("<i>" . $feature->organism_id->genus . " " . $feature->organism_id->species . "</i> (" .$feature->organism_id->common_name .")", "node/".$feature->organism_id->nid, array('html' => TRUE));
  } 
  else {
    $organism = $feature->organism_id->genus ." " . $feature->organism_id->species ." (" .$feature->organism_id->common_name .")";
  }
  $rows[] = array(
    array(
      'data' => 'Organism',
      'header' => TRUE
    ),
    $organism
  );
  // Seqlen row
  if($feature->seqlen > 0) {
    $rows[] = array(
      array(
        'data' => 'Sequence length',
        'header' => TRUE
      ),
      $feature->seqlen
    );
  }
  // allow site admins to see the feature ID
  if (user_access('access administration pages')) { 
    // Feature ID
    $rows[] = array(
      array(
        'data' => 'Feature ID',
        'header' => TRUE
      ),
      $feature->feature_id
    );
  }
  // Is Obsolete Row
  if($feature->is_obsolete == TRUE){
    $rows[] = array(
      array(
        'data' => '<div class="tripal_feature-obsolete">This feature is obsolete</div>',
        'colspan' => 2
      ),
    );
  }

  // the $table array contains the headers and rows array as well as other
  // options for controlling the display of the table.  Additional
  // documentation can be found here:
  // https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7
  $table = array(
    'header' => $headers,
    'rows' => $rows,
    'attributes' => array(
      'id' => 'tripal_feature-table-base',
    ),
    'sticky' => FALSE,
    'caption' => '',
    'colgroups' => array(),
    'empty' => '',
  );
  
  // once we have our table array structure defined, we call Drupal's theme_table()
  // function to generate the table.
  print theme_table($table); ?>
</div>