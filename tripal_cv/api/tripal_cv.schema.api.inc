<?php

/**
 * Implements hook_chado_schema_v1_2_tripal_obo_temp()
 * Purpose: To describe the structure of 'tripal_obo_temp' to tripal
 * @see tripal_core_chado_insert()
 * @see tripal_core_chado_update()
 * @see tripal_core_chado_select()
 *
 * @return
 *    An array describing the 'tripal_obo_temp' table
 *
 * @ingroup tripal_chado_v1.2_schema_api
 *
 */
function tripal_cv_chado_schema_v1_2_tripal_obo_temp() {
  $schema = array(
    'table' => 'tripal_obo_temp',
    'fields' => array(
      'id' => array(
        'type' => 'varchar',
        'length' => '255',
        'not null' => TRUE,
      ),
      'stanza' => array(
        'type' => 'text',
        'not null' => TRUE,
      ),
      'type' => array(
        'type' => 'varchar',
        'length' => '50',
        'not null' => TRUE,
      ),
    ),
    'indexes' => array(
      'tripal_obo_temp_idx0' => array('id'),
      'tripal_obo_temp_idx0' => array('type'),
    ),
    'unique keys' => array(
      'tripal_obo_temp_uq0' => array('id'),
    ),
  );
  return $schema;
}