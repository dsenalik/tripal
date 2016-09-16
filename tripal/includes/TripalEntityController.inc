<?php
/**
 * TripalEntityController extends DrupalDefaultEntityController.
 *
 * Our subclass of DrupalDefaultEntityController lets us add a few
 * important create, update, and delete methods.
 */
class TripalEntityController extends EntityAPIController {

  public function __construct($entityType) {
    parent::__construct($entityType);

  }

  /**
   * Create a Tripal data entity
   *
   * We first set up the values that are specific
   * to our data schema but then also go through the EntityAPIController
   * function.
   *
   * @param $type
   *   The machine-readable type of the entity.
   *
   * @return
   *   An object with all default fields initialized.
   */
  public function create(array $values = array()) {

    // Add some items to the values array passed to the constructor
    global $user;
    $values['uid'] = $user->uid;
    $values['created'] = time();
    $values['changed'] = time();
    $values['title'] = '';
    $values['type'] = 'TripalEntity';
    $values['nid'] = '';

    // Call the parent constructor.
    $entity = parent::create($values);

    // Allow modules to make additions to the entity when it's created.
    $modules = module_implements('entity_create');
    foreach ($modules as $module) {
      $function = $module . '_entity_create';
      $function($entity, $values['type']);
    }

    return $entity;

  }

  /**
   * Overrides EntityAPIController::delete().
   *
   * @param array $ids
   *    An array of the ids denoting which entities to delete.
   * @param DatabaseTransaction $transaction
   *    Optionally a DatabaseTransaction object to use. Allows overrides to pass in
   *    their transaction object.
   */
  public function delete($ids, $transaction = NULL) {

    if (!$transaction) {
      $transaction = db_transaction();
    }

    try {

      // First load the entity.
      $entities = entity_load('TripalEntity', $ids);

      // Then properly delete each one.
      foreach ($entities as $entity) {

        // Invoke hook_entity_delete().
        module_invoke_all('entity_delete', $entity, $entity->type);

        // Delete any field data for this entity.
        field_attach_delete('TripalEntity', $entity);

        // Delete the entity record from our base table.
        db_delete('tripal_entity')
          ->condition('id', $entity->id)
          ->execute();

      }
    }
    catch (Exception $e) {
      $transaction->rollback();
      watchdog_exception('tripal', $e);
      throw $e;
      return FALSE;
    }

    return TRUE;
  }

  /**
   * Sets the title for an entity.
   *
   * @param $entity
   * @param $title
   */
  public function setTitle($entity, $title = NULL) {

    // If no title was supplied then we should try to generate one using the
    // default format set by admins.
    if (!$title) {
      // Load the TripalBundle entity for this TripalEntity.
      // Get the format for the title based on the bundle of the entity.
      // And then replace all the tokens with values from the entity fields.
      $bundle_entity = tripal_load_bundle_entity(array('name' => $entity->bundle));
      $title = tripal_get_title_format($bundle_entity);
      $title = tripal_replace_entity_tokens($title, $entity, $bundle_entity);
    }
    // As long as we were able to determine a title, we should update it ;-).
    if ($title) {
      db_update('tripal_entity')
        ->fields(array(
          'title' => $title
        ))
        ->condition('id', $entity->id)
        ->execute();
    }
  }

  /**
   * Sets the URL alias for an entity.
   */
  public function setAlias($entity, $alias = NULL) {
    $source_url = "bio_data/$entity->id";

    // If no alias was supplied then we should try to generate one using the
    // default format set by admins.
    if (!$alias) {

      // Load the TripalBundle entity for this TripalEntity.
      $bundle_entity = tripal_load_bundle_entity(array('name' => $entity->bundle));

      // First get the format for the url alias based on the bundle of the entity.
      $alias = tripal_get_bundle_variable('url_format', $bundle_entity->id);

      // And then replace all the tokens with values from the entity fields.
      $alias = tripal_replace_entity_tokens($alias, $entity, $bundle_entity);
    }

    // If there was no defaults supplied by the admins
    // then we should gneerate our own using the term name and entity id.
    if (!$alias) {

      // Load the term for this TripalEntity.
      $term = entity_load('TripalTerm', array('id' => $entity->term_id));
      $term = reset($term);

      // Set a default based on the term name and entity id.
      $alias = str_replace(' ', '', $term->name) . '/[TripalEntity__entity_id]';

      // And then replace all the tokens with values from the entity fields.
      $alias = tripal_replace_entity_tokens($alias, $entity, $bundle_entity);
    }

    // Make sure the alias doesn't contain spaces.
    $alias = preg_replace('/\s+/','-',$alias);
    // Or any non alpha numeric characters.
    $alias = preg_replace('/[^a-zA-Z0-9\-\/]/','',$alias);
    $alias = preg_replace('/_/','-',$alias);

    if ($alias) {

      // Determine if this alias has already been used.
      $sql ='
        SELECT count(*) as num_alias
        FROM {url_alias}
        WHERE alias=:alias
      ';
      $num_aliases = db_query($sql, array(':alias' => $alias))->fetchField();

      // Either there isn't an alias yet so we just create one.
      // OR an Alias already exists but we would like to add a new one.
      if ($num_aliases == 0) {
        // First delete any previous alias' for this entity.
        // Then save the new one.

        // TODO: publishing an entity can be very slow if there are lots of
        // entries in the url_alias table, due to this type of
        // SQL statement that gets called somewhere by Drupal:
        // SELECT DISTINCT SUBSTRING_INDEX(source, '/', 1) AS path FROM url_alias.
        // Perhaps we should write our own SQL to avoid this issue.
        $values =  array(
          'source' => $source_url,
          'alias' => $alias,
          'language' => 'und',
        );
        drupal_write_record('url_alias', $values);
//        path_delete(array('source' => $source_url));
//        $path = array('source' => $source_url, 'alias' => $alias);
//        path_save($path);
      }
      // If there is only one alias matching then it might just be that we already
      // assigned this alias to this entity in a previous save.
      elseif ($num_aliases == 1) {

        $bundle_entity = tripal_load_bundle_entity(array('name' => $entity->bundle));

        // Check to see if the single alias is for the same entity and if not
        // warn the admin that the alias is already used (ie: not unique?)
        $sql = "
          SELECT count(*) as num_alias
          FROM {url_alias}
          WHERE alias=:alias AND source=:source
        ";
        $replace = array(':alias' => $alias, ':source' => $source_url);
        $same_alias = db_query($sql, $replace)->fetchField();
        if (!$same_alias) {
          $msg = 'The URL alias, %alias, already exists for another page. ' .
            'Please ensure the pattern supplied on the <a href="!link" ' .
            'target="_blank">%type Edit Page</a> under URL Path options is ' .
            'unique.';
          $msg_var = array(
            '%alias' => $alias,
            '!link' => url("admin/structure/bio_data/manage/$entity->bundle"),
            '%type' => $bundle_entity->label
          );
          tripal_report_error('trpentity', TRIPAL_WARNING, $msg, $msg_var);
          drupal_set_message(t($msg, $msg_var), 'warning');
        }
      }
      // If there are more then one alias' matching what we generated then there's
      // a real problem and we need to warn the administrator.
      else {
        $bundle_entity = tripal_load_bundle_entity(array('name' => $entity->bundle));

        $aliases = db_query('SELECT source FROM {url_alias} WHERE alias=:alias',
          array(':alias' => $alias))->fetchAll();
        $pages = array();
        foreach($aliases as $a) {
          $pages[] = $a->source;
        }

        $msg = 'The URL alias, %alias, already exists for multiple pages! '.
          'Please ensure the pattern supplied on the <a href="!link" ' .
          'target="_blank">%type Edit Page</a> under URL Path options is ' .
          'unique.';
        $msg_var = array(
          '%alias' => $alias,
          '!link' => url("admin/structure/bio_data/manage/$entity->bundle"),
          '%type' => $bundle_entity->label
        );
        drupal_set_message(t($msg, $msg_var), 'error');

        $msg .= ' This url alias has already been used for the following pages: %pages.
          You can manually delete alias\' using a combination of path_load() and path_delete().';
        $msg_var['%pages'] = implode(', ', $pages);
        tripal_report_error('trpentity', TRIPAL_ERROR, $msg, $msg_var);
      }
    }
  }

  /**
   * Saves a new entity.
   *
   * @param $entity
   *   A TripalEntity object to save.
   *
   * @return
   *   The saved entity object with updated properties.
   */
  public function save($entity) {
    global $user;
    $pkeys = array();

    $transaction  = db_transaction();
    try {
      // If our entity has no id, then we need to give it a
      // time of creation.
      if (empty($entity->id)) {
        $entity->created = time();
        $invocation = 'entity_insert';
      }
      else {
        $invocation = 'entity_update';
        $pkeys = array('id');
      }

      // Invoke hook_entity_presave().
      module_invoke_all('entity_presave', $entity, $entity->type);

      // Write out the entity record.
      $record = array(
        'term_id'   => $entity->term_id,
        'type'      => $entity->type,
        'bundle'    => $entity->bundle,
        'title'     => $entity->title,
        'uid'       => $user->uid,
        'created'   => $entity->created,
        'changed'   => time(),
      );
      if (property_exists($entity, 'nid') and $entity->nid) {
        $record['nid'] = $entity->nid;
      }
      if ($invocation == 'entity_update') {
        $record['id'] = $entity->id;
      }
      $success = drupal_write_record('tripal_entity', $record, $pkeys);
      if ($success == SAVED_NEW) {
        $entity->id = $record['id'];
      }

      // Now we need to either insert or update the fields which are
      // attached to this entity. We use the same primary_keys logic
      // to determine whether to update or insert, and which hook we
      // need to invoke.
      if ($invocation == 'entity_insert') {
        field_attach_insert('TripalEntity', $entity);
      }
      else {
        field_attach_update('TripalEntity', $entity);
      }

      // Set the title for this entity.
      $this->setTitle($entity);

      // Set the path/url alias for this entity.
      $this->setAlias($entity);

      // Invoke either hook_entity_update() or hook_entity_insert().
      module_invoke_all('entity_postsave', $entity, $entity->type);
      module_invoke_all($invocation, $entity, $entity->type);

      return $entity;
    }
    catch (Exception $e) {
      $transaction->rollback();
      watchdog_exception('tripal_core', $e);
      drupal_set_message("Could not save the entity: " . $e->getMessage(), "error");
      return FALSE;
    }
  }

  /**
   * Override the load function.
   *
   * A TripalEntity may have a large number of fields attached which may
   * slow down the loading of pages and web services.  Therefore, we only
   * want to attach fields that are needed.
   *
   * @param $ids
   *  The list of entity IDs to load.
   * @param $conditions
   *  The list of key/value filters for querying the entity.
   * @param $field_ids
   *  The list of numeric field IDs for fields that should be included.
   */
  public function load($ids = array(), $conditions = array(), $field_ids = array()) {

    $entities = array();

    // Revisions are not statically cached, and require a different query to
    // other conditions, so separate the revision id into its own variable.
    if ($this->revisionKey && isset($conditions[$this->revisionKey])) {
      $revision_id = $conditions[$this->revisionKey];
      unset($conditions[$this->revisionKey]);
    }
    else {
      $revision_id = FALSE;
    }

    // Create a new variable which is either a prepared version of the $ids
    // array for later comparison with the entity cache, or FALSE if no $ids
    // were passed. The $ids array is reduced as items are loaded from cache,
    // and we need to know if it's empty for this reason to avoid querying the
    // database when all requested entities are loaded from cache.
    $passed_ids = !empty($ids) ? array_flip($ids) : FALSE;

    // Try to load entities from the static cache.
    if ($this->cache && !$revision_id) {
      $entities = $this->cacheGet($ids, $conditions);
      // If any entities were loaded, remove them from the ids still to load.
      if ($passed_ids) {
        $ids = array_keys(array_diff_key($passed_ids, $entities));
      }
    }

    // Support the entitycache module if activated.
    if (!empty($this->entityInfo['entity cache']) && !$revision_id && $ids && !$conditions) {
      $cached_entities = EntityCacheControllerHelper::entityCacheGet($this, $ids, $conditions);
      // If any entities were loaded, remove them from the ids still to load.
      $ids = array_diff($ids, array_keys($cached_entities));
      $entities += $cached_entities;

      // Add loaded entities to the static cache if we are not loading a
      // revision.
      if ($this->cache && !empty($cached_entities) && !$revision_id) {
        $this->cacheSet($cached_entities);
      }
    }

    // Load any remaining entities from the database. This is the case if $ids
    // is set to FALSE (so we load all entities), if there are any ids left to
    // load or if loading a revision.
    if (!($this->cacheComplete && $ids === FALSE && !$conditions) && ($ids === FALSE || $ids || $revision_id)) {
      $queried_entities = array();
      foreach ($this->query($ids, $conditions, $revision_id) as $record) {
        // Skip entities already retrieved from cache.
        if (isset($entities[$record->{$this->idKey}])) {
          continue;
        }

        // For DB-based entities take care of serialized columns.
        if (!empty($this->entityInfo['base table'])) {
          $schema = drupal_get_schema($this->entityInfo['base table']);

          foreach ($schema['fields'] as $field => $info) {
            if (!empty($info['serialize']) && isset($record->$field)) {
              $record->$field = unserialize($record->$field);
              // Support automatic merging of 'data' fields into the entity.
              if (!empty($info['merge']) && is_array($record->$field)) {
                foreach ($record->$field as $key => $value) {
                  $record->$key = $value;
                }
                unset($record->$field);
              }
            }
          }
        }

        $queried_entities[$record->{$this->idKey}] = $record;
      }
    }

    // Pass all entities loaded from the database through $this->attachLoad(),
    // which attaches fields (if supported by the entity type) and calls the
    // entity type specific load callback, for example hook_node_load().
    if (!empty($queried_entities)) {
      $this->attachLoad($queried_entities, $revision_id, $field_ids);
      $entities += $queried_entities;
    }

    // Entitycache module support: Add entities to the entity cache if we are
    // not loading a revision.
    if (!empty($this->entityInfo['entity cache']) && !empty($queried_entities) && !$revision_id) {
      EntityCacheControllerHelper::entityCacheSet($this, $queried_entities);
    }

    if ($this->cache) {
      // Add entities to the cache if we are not loading a revision.
      if (!empty($queried_entities) && !$revision_id) {
        $this->cacheSet($queried_entities);

        // Remember if we have cached all entities now.
        if (!$conditions && $ids === FALSE) {
          $this->cacheComplete = TRUE;
        }
      }
    }
    // Ensure that the returned array is ordered the same as the original
    // $ids array if this was passed in and remove any invalid ids.
    if ($passed_ids && $passed_ids = array_intersect_key($passed_ids, $entities)) {
      foreach ($passed_ids as $id => $value) {
        $passed_ids[$id] = $entities[$id];
      }
      $entities = $passed_ids;
    }
    return $entities;
  }

  /**
   * Override the attachLoad function.
   *
   * A TripalEntity may have a large number of fields attached which may
   * slow down the loading of pages and web services.  Therefore, we only
   * want to attach fields that are needed.
   *
   *
   *
   * @param $queried_entities
   *   The list of queried
   * @param $revision_id
   * @param $field_ids
   */
  protected function attachLoad(&$queried_entities, $revision_id = FALSE,
      $field_ids = array()) {

    // Get the list of fields for later user.
    $fields = field_info_fields();
    // A temporary holding place for field instances (so we don't have
    // to keep looking them up in our loop below).
    $instances = array();

    // Add in the content_type field by default. It should always be
    // attached.
    $ids = array();
    $ct = field_info_field('content_type');
    $ids[] = $ct['id'];
    $field_ids += $ids;


    // Attach fields.
    if ($this->entityInfo['fieldable']) {
      if ($revision_id) {
        $function = 'field_attach_load_revision';
      }
      else {
        $function = 'field_attach_load';
      }
      foreach ($queried_entities as $entity) {
        $entity_ids = array();
        $entity_ids += $field_ids;
        // Iterate through the fields and find those that are set to
        // 'auto_attach' and which are attached to this bundle.
        foreach ($fields as $field) {
          $field_name =  $field['field_name'];
          if (array_key_exists('TripalEntity', $field['bundles'])) {
            foreach ($field['bundles']['TripalEntity'] as $bundle_name) {
              if ($bundle_name == $entity->bundle) {
                // Get the instance of this field for this bundle.
                if (!array_key_exists($field_name . '-' . $bundle_name, $instances)) {
                  $instance = field_info_instance('TripalEntity', $field['field_name'], $bundle_name);
                  $instances[$field_name . '-' . $bundle_name] = $instance;
                }
                else {
                  $instance = $instances[$field_name . '-' . $bundle_name];
                }
                // If the 'auto_attach' is set then add this field id to our
                // list.
                if (array_key_exists('settings', $instance) and
                    array_key_exists('auto_attach', $instance['settings']) and
                    $instance['settings']['auto_attach'] === TRUE) {
                  $entity_ids[] = $field['id'];
                }
                // If the field is not auto attached then we want to add an
                // empty default valuea and add an 'unattached' key to
                // clue in other things that there may be data, it's just
                // not attached.
                else {
//                   $value = array(
//                     'und' => array(
//                       0 => array(
//                         'value' => '',
//                         '#unattached' => TRUE,
//                       ),
//                     ),
//                   );
//                   $entity->$field_name = $value;
                }
              }
            }
          }
        }

        // Iterate through each field that should be added and attach it.
        foreach ($entity_ids as $field_id) {
          $options = array();
          $options['field_id'] = $field_id;
          $function($this->entityType, array($entity->id => $entity), FIELD_LOAD_CURRENT, $options);
        }
      }
    }

    // Call hook_entity_load().
    foreach (module_implements('entity_load') as $module) {
      $function = $module . '_entity_load';
      $function($queried_entities, $this->entityType);
    }
    // Call hook_TYPE_load(). The first argument for hook_TYPE_load() are
    // always the queried entities, followed by additional arguments set in
    // $this->hookLoadArguments.
    $args = array_merge(array($queried_entities), $this->hookLoadArguments);
    foreach (module_implements($this->entityInfo['load hook']) as $module) {
      call_user_func_array($module . '_' . $this->entityInfo['load hook'], $args);
    }
  }
}
