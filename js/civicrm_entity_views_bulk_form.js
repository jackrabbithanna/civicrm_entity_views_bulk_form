(function ($) {
    // Polyfill for jQuery less than 1.6.
    if (typeof $.fn.prop != 'function') {
        jQuery.fn.extend({
            prop: jQuery.fn.attr
        });
    }

    Drupal.behaviors.cevbf = {
        attach: function(context) {
            $('.cevbf-views-form', context).each(function() {
                Drupal.cevbf.initTableBehaviors(this);
               // Drupal.cevbf.initGenericBehaviors(this);
            });
        }
    }

    Drupal.cevbf = Drupal.cevbf || {};
    Drupal.cevbf.initTableBehaviors = function(form) {
        // This is the "select all" checkbox in (each) table header.
        $('.views-bulk-civicrm-entity-form-select-all', form).click(function() {
            var table = $(this).closest('table')[0];
            $('.vviews-bulk-civicrm-entity-form-select:not(:disabled)', table).prop('checked', this.checked);
        });

        $('.views-bulk-civicrm-entity-form-select', form).click(function() {
            var table = $(this).closest('table.views-table')[0];
            if (!this.checked) {
                $('.views-bulk-civicrm-entity-form-select-all', table).prop('checked', false);
            }
        });
    }

   Drupal.cevbf.initGenericBehaviors = function(form) {

   }

})(jQuery);