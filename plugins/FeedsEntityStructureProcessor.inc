<?php

define('FEEDS_ENTITY_STRUCTURE_UPDATE_EXISTING_BASE_BY_TYPE', 3);


/**
 * Creates entity structure from feed items.
 */
class FeedsEntityStructureProcessor extends FeedsProcessor {

  /**
   * Implements parent::entityInfo().
   */
  protected function entityInfo() {
    $info = parent::entityInfo();
    $info['label plural'] = t('Fields');
    return $info;
  }

  protected function parseCardinality($cardinality) {
    if (empty($cardinality)) {
      // No data, do not overwrite.
      return '';
    }
    elseif (intval($cardinality)) {
      return $cardinality;
    }
    elseif (strtolower(substr($cardinality, 0, 3)) == 'inf') {
      return -1;
    }
    else {
      return 1;
    }
  }

  protected function parseAllowedValues($options) {
    $options = str_getcsv($options);
    $options = array_map('trim', $options);
    $options = array_combine($options, $options);
    return $options;
  }

  /**
   * Creates the base fields.
   *
   * @param array $field_data
   *   An associative array that contains the data for this field.
   * @param array $headers
   *   An associative array where the keys are the header names and the values are
   *   the header indices.
   *
   * @return array|bool|mixed|void
   *   Returns either the result of field_info_field, the newly created field, or
   *   FALSE if there were errors.
   */
  protected function updateFieldBase(FeedsSource $source, FeedsParserResult $parser_result, $field_base) {
    $skip_new = $this->config['insert_new'] == FEEDS_SKIP_NEW;
    $skip_existing_base = $this->config['update_existing_base'] == FEEDS_SKIP_EXISTING;
    $skip_existing_instance = $this->config['update_existing_instance'] == FEEDS_SKIP_EXISTING;

    $field_name = $this->getSourceValueFromTarget($source, $parser_result, 'field_name');
    $field_name = $this->processMachineName($field_name);

    $field_base_properties = array(
      'type' => $this->getSourceValueFromTarget($source, $parser_result, 'type'),
      'cardinality' => $this->parseCardinality($this->getSourceValueFromTarget($source, $parser_result, 'cardinality')),

    );

    $field_base = array_merge($field_base, $field_base_properties);

    $allowed_values = $this->parseAllowedValues($this->getSourceValueFromTarget($source, $parser_result, 'allowed_values'));
    if (!empty($allowed_values)) {
      $field_base['settings']['allowed_values'] = $allowed_values;
      $field_base['settings']['allowed_values_function'] = '';
    }
    $field_base_properties_settings = array(
      'options' => $this->parseOptions($this->getSourceValueFromTarget($source, $parser_result, 'allowed_values')),
    );


    drupal_alter('feeds_entity_structure_field_base', $source, $parser_result, $field_base);
    return $field_base;
  }

  /**
   * Attaches a field to an instance, with settings, widgets, etc ready to go.
   *
   * @param array $field_data
   *   An associative array that contains the data for this field.
   * @param string $entity_type
   *   The entity type to attach to.
   * @param string $bundle
   *   The bundle type to attach to.
   * @param string $field_name
   *   The name of the field.
   * @param array $headers
   *   An associative array where the keys are the header names and the values are
   *   the header indices.
   *
   * @return bool|mixed
   *   Either returns the new entity instance or FALSE if there were errors.
   */
  protected function initialize_instance(&$field_data, $entity_type, $bundle, $field_name, $headers) {
    $final_field_data = array(
      'entity_type' => $entity_type,
      'bundle' => $bundle,
      'field_name' => $field_name,
    );
    foreach (array_keys($headers) as $field_property_name) {
      drupal_alter('csv2fields_field_instance_' . $field_property_name, $final_field_data, $field_data);
    }
    drupal_alter('csv2fields_field_instance', $final_field_data, $field_data);
    if (isset($final_field_data['errors']) && count($final_field_data['errors'])) {
      foreach ($final_field_data['errors'] as $error) {
        drush_log(dt('@error on row @row_number', array(
          '@row_number' => $field_data['row_number'],
          '@error' => $error,
        )), 'error');
      }
      return FALSE;
    }
    else {
      return field_create_instance($final_field_data);
    }
  }

  /**
   * Process the result of the parsing stage.
   *
   * @param FeedsSource $source
   *   Source information about this import.
   * @param FeedsParserResult $parser_result
   *   The result of the parsing stage.
   */
  public function process(FeedsSource $source, FeedsParserResult $parser_result) {
    $state = $source->state(FEEDS_PROCESS);
    if (!isset($state->removeList) && $parser_result->items) {
      $this->initEntitiesToBeRemoved($source, $state);
    }

    $skip_new = $this->config['insert_new'] == FEEDS_SKIP_NEW;
    $skip_existing_base = $this->config['update_existing_base'] == FEEDS_SKIP_EXISTING;
    $skip_existing_instance = $this->config['update_existing_instance'] == FEEDS_SKIP_EXISTING;

    while ($item = $parser_result->shiftItem()) {
      // Check if this item already exists. @TODO

      $field_name = $this->getSourceValueFromTarget($source, $parser_result, 'field_name');
      $field_name = $this->processMachineName($field_name);
      $group_name = $this->getSourceValueFromTarget($source, $parser_result, 'field_group_name');
      $group_name = $this->processMachineName($group_name);
      $entity_name = $this->getSourceValueFromTarget($source, $parser_result, 'entity');
      $bundle_name = $this->getSourceValueFromTarget($source, $parser_result, 'bundle');
      // @TODO remove from delete list BEFORE checking anything else.

      if (!empty($field_name)) {
        $field_base = $this->existingFieldBase($field_name);
        if (empty($field_base) && !$skip_new) {
          $field_base = array(
            'field_name' => $field_name,
            'cardinality' => 1,
          );
          $this->updateFieldBase($source, $parser_result, $field_base);
          field_create_field($field_base);
        }
        elseif (!empty($field_base)) {
          $type = $this->getSourceValueFromTarget($source, $parser_result, 'type');
          if (($skip_existing_base == FEEDS_ENTITY_STRUCTURE_UPDATE_EXISTING_BASE_BY_TYPE && $type == $field_base['type'])
          || $skip_existing_base == FEEDS_UPDATE_EXISTING) {
            $this->updateFieldBase($source, $parser_result, $field_base);
            field_update_field($field_base);
          }
        }

        $field_instance = $this->existingFieldInstance($field_name, $entity_name, $entity_bundle);
        if (empty($field_base)) {
          // Create a stub
        }
        else {
          // Check and see if this should be updated, and update.
        }
      }
      elseif (!empty($group_name)) {

      }

      continue;
      // If it's included in the feed, it must not be removed on clean.
      if ($entity_id) {
        unset($state->removeList[$entity_id]);
      }
      module_invoke_all('feeds_entity_structure_before_update', $source, $item, $entity_id);

      // If it exists, and we are not updating, or if it does not exist, and we
      // are not inserting, pass onto the next item.
      if (($entity_id && $skip_existing) || (!$entity_id && $skip_new)) {
        continue;
      }

      try {
        $hash = $this->hash($item);
        $changed = $hash !== $this->getHash($entity_id);

        // Do not proceed if the item exists, has not changed, and we're not
        // forcing the update.
        if ($entity_id && !$changed && !$this->config['skip_hash_check']) {
          continue;
        }

        // Load an existing entity.
        if ($entity_id) {
          $entity = $this->entityLoad($source, $entity_id);

          // The feeds_item table is always updated with the info for the most
          // recently processed entity. The only carryover is the entity_id.
          $this->newItemInfo($entity, $source->feed_nid, $hash);
          $entity->feeds_item->entity_id = $entity_id;
          $entity->feeds_item->is_new = FALSE;
        }

        // Build a new entity.
        else {
          $entity = $this->newEntity($source);
          $this->newItemInfo($entity, $source->feed_nid, $hash);
        }

        // Set property and field values.
        $this->map($source, $parser_result, $entity);
        $this->entityValidate($entity);

        // Allow modules to alter the entity before saving.
        module_invoke_all('feeds_entity_structure_presave', $source, $entity, $item, $entity_id);

        // Enable modules to skip saving at all.
        if (!empty($entity->feeds_item->skip)) {
          continue;
        }

        // This will throw an exception on failure.
        $this->entitySaveAccess($entity);
        $this->entitySave($entity);

        // Allow modules to perform operations using the saved entity data.
        // $entity contains the updated entity after saving.
        module_invoke_all('feeds_after_save', $source, $entity, $item, $entity_id);

        // Track progress.
        if (empty($entity_id)) {
          $state->created++;
        }
        else {
          $state->updated++;
        }
      }

      // Something bad happened, log it.
      catch (Exception $e) {
        $state->failed++;
        drupal_set_message($e->getMessage(), 'warning');
        list($message, $arguments) = $this->createLogEntry($e, $entity, $item);
        $source->log('import', $message, $arguments, WATCHDOG_ERROR);
      }
    }
    return;
    // Set messages if we're done.
    if ($source->progressImporting() != FEEDS_BATCH_COMPLETE) {
      return;
    }
    // Remove not included items if needed.
    // It depends on the implementation of the clean() method what will happen
    // to items that were no longer in the source.
    $this->clean($state);
    $info = $this->entityInfo();
    $tokens = array(
      '@entity' => 'field',
      '@entities' => 'fields',
    );
    $messages = array();
    if ($state->created) {
      $messages[] = array(
       'message' => format_plural(
          $state->created,
          'Created @number @entity.',
          'Created @number @entities.',
          array('@number' => $state->created) + $tokens
        ),
      );
    }
    if ($state->updated) {
      $messages[] = array(
       'message' => format_plural(
          $state->updated,
          'Updated @number @entity.',
          'Updated @number @entities.',
          array('@number' => $state->updated) + $tokens
        ),
      );
    }
    if ($state->unpublished) {
      $messages[] = array(
        'message' => format_plural(
            $state->unpublished,
            'Unpublished @number @entity.',
            'Unpublished @number @entities.',
            array('@number' => $state->unpublished) + $tokens
        ),
      );
    }
    if ($state->blocked) {
      $messages[] = array(
        'message' => format_plural(
          $state->blocked,
          'Blocked @number @entity.',
          'Blocked @number @entities.',
          array('@number' => $state->blocked) + $tokens
        ),
      );
    }
    if ($state->deleted) {
      $messages[] = array(
       'message' => format_plural(
          $state->deleted,
          'Removed @number @entity.',
          'Removed @number @entities.',
          array('@number' => $state->deleted) + $tokens
        ),
      );
    }
    if ($state->failed) {
      $messages[] = array(
       'message' => format_plural(
          $state->failed,
          'Failed importing @number @entity.',
          'Failed importing @number @entities.',
          array('@number' => $state->failed) + $tokens
        ),
        'level' => WATCHDOG_ERROR,
      );
    }
    if (empty($messages)) {
      $messages[] = array(
        'message' => t('There are no new @entities.', array('@entities' => strtolower($info['label plural']))),
      );
    }
    foreach ($messages as $message) {
      drupal_set_message($message['message']);
      $source->log('import', $message['message'], array(), isset($message['level']) ? $message['level'] : WATCHDOG_INFO);
    }
  }

  protected function existingFieldBase($field_name) {
    if (!empty($field_name)) {
      $field = field_info_field($field_name);
      return $field;
    }
    return FALSE;
  }

  protected function getSourceValueFromTarget(FeedsSource $source, FeedsParserResult $parser_result, $target) {
    $mappings = $this->getMappings();
    foreach($mappings as $mid => $mapping) {
      if ($mapping['target'] == $target) {
        $item = $parser_result->currentItem();
        $element_key = $mapping['source'];
        return isset($item[$element_key]) ? $item[$element_key] : '';
      }
    }

    // Normally I would make this FALSE, but parsers seem to prefer ''
    return '';
  }

  /**
   * Creates a group.
   *
   * @param array $group_data
   *   An associative array containing all of the data for this group.
   * @param string $entity_type
   *   The entity type to attach this group to.
   * @param string $bundle
   *   The bundle to attach this group to.
   * @param string $group_name
   *   The group's name.
   * @param array $headers
   *   An associative array where the keys are the header names and the values are
   *   the header indices.
   *
   * @return bool
   */
  protected function initialize_group(&$group_data, $entity_type, $bundle, $group_name, $headers) {
    $final_group_data = array();
    foreach (array_keys($headers) as $group_property_name) {
      drupal_alter('csv2fields_group_' . $group_property_name, $final_group_data, $group_data);
    }
    drupal_alter('csv2fields_group', $final_group_data, $group_data);
    if (isset($final_group_data['errors']) && count($final_group_data['errors'])) {
      foreach ($final_group_data['errors'] as $error) {
        drush_log(dt('@error on row @row_number', array(
          '@row_number' => $group_data['row_number'],
          '@error' => $error,
        )), 'error');
      }
      return FALSE;
    }
    else {
      $group = (object) array(
        'identifier' => dt('@fieldName|@type|@bundle|form', array(
          '@fieldName' => $group_data['field_group_name'],
          '@type' => $entity_type,
          '@bundle' => $bundle,
        )),
        'group_name' => $group_data['field_group_name'],
        'entity_type' => $entity_type,
        'bundle' => $bundle,
        'mode' => 'form',
        'label' => $final_group_data['label'],
        'children' => array(),
        'weight' => $final_group_data['widget']['weight'],
        'format_type' => 'fieldset',
        'format_settings' => array(
          'formatter' => 'collapsible',
          'instance_settings' => array(
            'description' => '',
            'classes' => '',
            'required_fields' => 1,
          ),
        ),
      );
      field_group_group_save($group);
      ctools_include('export');
      ctools_export_crud_enable('field_group', $group->identifier);
      return $group;
    }
  }

  /**
   * @param string $element_name
   *   The name of the field or group to add.
   * @param string $field_group
   *   The name of the field group to add to.
   * @param string $entity_type
   *   The name of the entity.
   * @param string $bundle
   *   The name of the bundle.
   */
  protected function add_element_to_group($element_name, $field_group, $entity_type, $bundle) {
    $groups = field_group_read_groups(array(
      'entity_type' => $entity_type,
      'bundle'      => $bundle,
      'view_mode'   => 'full',
    ));
    $group = $groups[$entity_type][$bundle]['form'][$field_group];
    if (!empty($group)) {
      $group->children[] = $element_name;
      field_group_group_save($group);
    }
    else {
      drush_log("Couldn't find the group @group", array(
        '@group' => $field_group,
      ), 'error');
    }
  }

  /**
   * Converts a csv row array into an associative array based on the column headers.
   *
   * @param $field_data
   *   The csv row array that contains the data.
   * @param $headers
   *   An associative array where the column name is the key, and the column number
   *   is the value.
   *
   * @return array
   *   An associative array where each column is the key and the data for the row
   *   is the value.
   */
  protected function get_data($field_data, $headers) {
    $final_field_data = array();
    foreach ($headers as $property_name => $index) {
      $final_field_data[$property_name] = trim($field_data[$index]);
    }
    return $final_field_data;
  }

  /**
   * Override parent::configDefaults().
   */
  public function configDefaults() {
    return array(
      'entity' => 'node',
      'bundle' => 'page',
      'update_existing_bases' => FEEDS_SKIP_EXISTING,
      'update_existing_instances' => FEEDS_SKIP_EXISTING,
      'mappings' => array(
        0 => array(
          'source' => 'entity',
          'target' => 'entity',
          'unique' => FALSE,
          'language' => 'und',
        ),
        1 => array(
          'source' => 'bundle',
          'target' => 'bundle',
          'unique' => FALSE,
          'language' => 'und',
        ),
        2 => array(
          'source' => 'field_name',
          'target' => 'field_name',
          'unique' => 1,
          'language' => 'und',
        ),
        3 => array(
          'source' => 'type',
          'target' => 'type',
          'unique' => FALSE,
          'language' => 'und',
        ),
        4 => array(
          'source' => 'label',
          'target' => 'label',
          'unique' => FALSE,
          'language' => 'und',
        ),
        5 => array(
          'source' => 'cardinality',
          'target' => 'cardinality',
          'unique' => FALSE,
          'language' => 'und',
        ),
        6 => array(
          'source' => 'allowed_values',
          'target' => 'allowed_values',
          'unique' => FALSE,
          'language' => 'und',
        ),
        7 => array(
          'source' => 'description',
          'target' => 'description',
          'unique' => FALSE,
          'language' => 'und',
        ),
        8 => array(
          'source' => 'default_value',
          'target' => 'default_value',
          'unique' => FALSE,
          'language' => 'und',
        ),
        9 => array(
          'source' => 'required',
          'target' => 'required',
          'unique' => FALSE,
          'language' => 'und',
        ),
        10 => array(
          'source' => 'widget_type',
          'target' => 'widget_type',
          'unique' => FALSE,
          'language' => 'und',
        ),
        11 => array(
          'source' => 'settings_text_processing',
          'target' => 'settings_text_processing',
          'unique' => FALSE,
          'language' => 'und',
        ),
        12 => array(
          'source' => 'settings_prefix',
          'target' => 'settings_prefix',
          'unique' => FALSE,
          'language' => 'und',
        ),
        13 => array(
          'source' => 'settings_suffix',
          'target' => 'settings_suffix',
          'unique' => FALSE,
          'language' => 'und',
        ),
        14 => array(
          'source' => 'field_group_name',
          'target' => 'field_group_name',
          'unique' => FALSE,
          'language' => 'und',
        ),
        15 => array(
          'source' => 'field_group_parent',
          'target' => 'field_group_parent',
          'unique' => FALSE,
        ),
      ),
    ) + parent::configDefaults();
  }

  public function configForm(&$form_state) {
    //drupal_page_is_cacheable(FALSE);

    $form = parent::configForm($form_state);
    $entities = entity_get_info(NULL);

    $entities = array_filter($entities, function($entity) {
      return !empty($entity['fieldable']);
    });

    $entity_options = array_map(function($entity) {
      return $entity['label'];
    }, $entities);
    $bundle_options = array_map(function($bundle) {
      return $bundle['label'];
    }, $entities[$this->entityType()]['bundles']);
    if (!sizeof($bundle_options)) {
      $bundle_options[$this->entityType()] = $entities[$this->entityType()]['label'];
    }
    dsm($form);
    $form_state['rebuild'] = TRUE;
    $form['entity'] = array(
      '#type' => 'select',
      '#title' => t('Default entity'),
      '#description' => t('Select the default entity to add fields to. This can be overridden by mapping to an entity.'),
      '#options' => $entity_options,
      '#default_value' => $this->entityType(),
      '#weight' => -50,
      '#required' => TRUE,
      '#ajax' => array(
        'callback' => 'csv2fields_bundle_callback',
        'wrapper' => 'feeds-entity-structure-processor-bundle',
        'effect' => 'fade',
      ),
    );

    unset($form['input_format']);

    $form['bundle'] = array(
      '#type' => 'select',
      '#title' => t('Default @entity type', array(
        '@entity' => $entities[$this->entityType()]['label'],
      )),
      '#description' => t('Select the default entity to add fields to. This can be overridden by mapping to an entity.'),
      '#options' => $bundle_options,
      '#default_value' => $this->config['bundle'],
      '#prefix' => '<div id="feeds-entity-structure-processor-bundle">',
      '#suffix' => '</div>',
      '#weight' => -49,
      '#required' => TRUE,
    );

    unset($form['update_existing']);

    $form['update_existing_bases'] = array(
      '#type' => 'radios',
      '#title' => t('Update existing field bases'),
      '#description' =>
        t('Existing field bases will be determined using mappings that are a "unique target".'),
      '#options' => array(
        FEEDS_SKIP_EXISTING => t('Do not update existing field bases'),
        FEEDS_UPDATE_EXISTING => t('Update existing field bases'),
        FEEDS_ENTITY_STRUCTURE_UPDATE_EXISTING_BASE_BY_TYPE => t('Update existing field bases only if the type is the same'),
      ),
      '#default_value' => $this->config['update_existing_bases'],
    );

    $form['update_existing_instances'] = array(
      '#type' => 'radios',
      '#title' => t('Update existing field instances'),
      '#description' =>
        t('Existing field instances will be determined using mappings that are a "unique target".'),
      '#options' => array(
        FEEDS_SKIP_EXISTING => t('Do not update existing field instances'),
        FEEDS_UPDATE_EXISTING => t('Update existing field instances'),
      ),
      '#default_value' => $this->config['update_existing_instances'],
    );

    $form['update_non_existent']['#options']['delete'] = t('Delete non-existent field instances, and delete field base if not used on other entities');

    return $form;
  }

  /**
   * Return available mapping targets.
   */
  public function getMappingTargets() {
    $targets = array();
    $targets['field_name'] = array(
      'name' => t('Field Machine Name'),
      'description' => t('The unique machine name of the field.'),
      'optional_unique' => TRUE,
    );
    $targets['label'] = array(
      'name' => t('Label'),
      'description' => t('The display label.'),
      'optional_unique' => TRUE,
    );
    $targets['type'] = array(
      'name' => t('Field Type'),
      'description' => t('The type of field (list_text, datetime, text_long, number_float, etc).'),
    );
    $targets['bundle'] = array(
      'name' => t('Bundle'),
      'description' => t('The machine name of the bundle to add this field to.'),
    );
    $targets['entity'] = array(
      'name' => t('Entity Type'),
      'description' => t('The machine name of the entity to add this field to.'),
    );
    $targets['description'] = array(
      'name' => t('Help Description'),
      'description' => t('Field help text.'),
    );
    $targets['default_value'] = array(
      'name' => t('Default Value'),
      'description' => t('The default value of a field.'),
    );
    $targets['required'] = array(
      'name' => t('Required'),
      'description' => t('Whether a field is required.'),
    );
    $targets['widget_type'] = array(
      'name' => t('Widget Type'),
      'description' => t('If not empty, this will select a widget type other than the default for the type.'),
    );
    $targets['cardinality'] = array(
      'name' => t('Cardinality'),
      'description' => t('The number of values allowed. Use -1 or any string that starts with \'inf\' (not case-sensitive) to indicate unlimited values.'),
    );
    $targets['allowed_values'] = array(
      'name' => t('Allowed Values'),
      'description' => t('A comma-separated list of possible values for list_* types.'),
    );
    $targets['settings_prefix'] = array(
      'name' => t('Prefix'),
      'description' => t('The prefix for a field'),
    );
    $targets['settings_suffix'] = array(
      'name' => t('Suffix'),
      'description' => t('The suffix for a field'),
    );
    $targets['settings_text_processing'] = array(
      'name' => t('Text Filtering'),
      'description' => t('Whether a text_long field should be plain or filterable. Leave empty or 0 (zero) for not plain, put anything else for filtered.'),
    );
    if (module_exists('field_group')) {
      $targets['field_group_name'] = array(
        'name' => t('Field Group'),
        'description' => t('If not empty, this element will be converted into a group instead of a field. This should be the machine name of the group.'),
      );
      $targets['field_group_parent'] = array(
        'name' => t('Field Group Parent'),
        'description' => t('If not empty, this field or group will be placed inside the given group. The group must have been defined already, or it must be in a higher row in the csv file than the element being placed inside it. Groups may be nested.'),
      );
    }

    // Add hooks, but do not actually
    $original_targets = $targets;
    $this->getHookTargets($targets);
    foreach (array_keys($targets) as $target_id) {
      if (!isset($original_targets[$target_id])) {
        unset($targets[$target_id]);
      }
    }
    return $targets;
  }

  /**
   * Allows other modules to expose targets.
   *
   * @param array &$targets
   *   The existing target array.
   */
  protected function getHookTargets(array &$targets) {
    self::loadMappers();

    // This is a hacky way to make sure that only entity structure mappings are
    // added, instead of mappings for the $entity_type.
    $entity_type = 'entity_structure';
    $bundle = NULL;
    $targets += module_invoke_all('feeds_processor_targets', $entity_type, $bundle);

    drupal_alter('feeds_processor_targets', $targets, $entity_type, $bundle);
  }

  public function entityType() {
    if (!empty($this->config['entity'])) {
      return $this->config['entity'];
    }
    else {
      return 'node';
    }
  }

  protected function entitySave($entity) {
    //do nothing here
  }

  protected function entityDeleteMultiple($entity_ids) {
    //do nothing here
  }

  protected function processMachineName($field_name) {
    return str_replace(' ', '_', strtolower(trim($field_name)));
  }
}
