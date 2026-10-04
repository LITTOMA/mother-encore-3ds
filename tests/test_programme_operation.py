"""Bounded recipe reuse cannot escape conversion admission or failure gates."""
import copy
from pathlib import Path
import unittest
from unittest.mock import patch
from tools import programme_lowering_recipe as recipe


class ProgrammeOperationTests(unittest.TestCase):
    def setUp(self):
        self.value = {'schema': 1, 'items': [1]}
        self.reader = patch.object(recipe, 'read', side_effect=lambda root: copy.deepcopy(self.value))
        self.checker = patch.object(recipe, 'checked', side_effect=lambda value, root: value)
        self.reader.start()
        self.checked = self.checker.start()
        self.addCleanup(self.reader.stop)
        self.addCleanup(self.checker.stop)

    def test_nested_calls_reuse_admission_and_return_independent_values(self):
        @recipe.operation
        def inner(root=recipe.ROOT):
            value = recipe.load(root)
            value['items'].append(2)
            return recipe.load(root)
        @recipe.operation
        def outer(root=recipe.ROOT):
            recipe.load(root)
            return inner(root)
        self.assertEqual(outer(), self.value)
        self.assertEqual(self.checked.call_count, 2)  # admission + final gate
        outer()
        self.assertEqual(self.checked.call_count, 4)  # no cross-operation cache

    def test_recipe_edit_inside_operation_is_rejected_immediately(self):
        @recipe.operation
        def convert(root=recipe.ROOT):
            recipe.load(root)
            self.value['items'].append(3)
            return recipe.load(root)
        with self.assertRaisesRegex(ValueError, 'changed during conversion'):
            convert()
        self.assertIsNone(recipe._operation.get())

    def test_last_recipe_edit_is_rejected_before_return(self):
        @recipe.operation
        def convert(root=recipe.ROOT):
            recipe.load(root)
            self.value['items'].append(3)
            return 'must not commit'
        with self.assertRaisesRegex(ValueError, 'changed during conversion'):
            convert()

    def test_changed_source_is_rechecked_before_return(self):
        @recipe.operation
        def convert(root=recipe.ROOT):
            recipe.load(root)
            return 'must not commit'
        self.checked.side_effect = [copy.deepcopy(self.value), ValueError('Changed pinned source')]
        with self.assertRaisesRegex(ValueError, 'Changed pinned source'):
            convert()

    def test_failure_restores_context_and_next_call_checks_again(self):
        @recipe.operation
        def convert(root=recipe.ROOT):
            recipe.load(root)
            raise ValueError('original failure')
        with self.assertRaisesRegex(ValueError, 'original failure'):
            convert()
        self.assertIsNone(recipe._operation.get())
        self.checked.side_effect = ValueError('new invalid source')
        with self.assertRaisesRegex(ValueError, 'new invalid source'):
            recipe.load()

    def test_nested_other_root_has_its_own_admission(self):
        @recipe.operation
        def convert(root=recipe.ROOT):
            return recipe.load(root)
        @recipe.operation
        def outer(root=recipe.ROOT):
            recipe.load(root)
            convert(Path(root) / 'other')
            return recipe.load(root)
        self.assertEqual(outer(), self.value)
        self.assertEqual(self.checked.call_count, 4)
        self.assertIsNone(recipe._operation.get())

    def test_receipt_failure_before_recipe_use_keeps_original_error(self):
        @recipe.operation
        def convert(root=recipe.ROOT):
            raise ValueError('bad receipt')
        with self.assertRaisesRegex(ValueError, 'bad receipt'):
            convert()
        self.checked.assert_not_called()


if __name__ == '__main__':
    unittest.main()
