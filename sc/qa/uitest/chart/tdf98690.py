# -*- tab-width: 4; indent-tabs-mode: nil; py-indent-offset: 4 -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
from uitest.framework import UITestCase
from libreoffice.uno.propertyvalue import mkPropertyValues
from uitest.uihelper.common import get_url_for_data_file

class tdf98690(UITestCase):

  def test_tdf98690(self):
    with self.ui_test.load_file(get_url_for_data_file("tdf98690.xlsx")) as calc_doc:
        xCalcDoc = self.xUITest.getTopFocusWindow()
        gridwin = xCalcDoc.getChild("grid_window")

        gridwin.executeAction("SELECT", mkPropertyValues({"OBJECT": "Chart 2"}))

        self.xUITest.executeCommand(".uno:Copy")

    with self.ui_test.load_empty_file("calc") as calc_document:
        xCalcDoc = self.xUITest.getTopFocusWindow()
        gridwin = xCalcDoc.getChild("grid_window")

        self.xUITest.executeCommand(".uno:Paste")

        xData = calc_document.Sheets[0].Charts[0].getEmbeddedObject().Data

        aExpectedRowDescriptions = tuple(str(i) for i in range(1, 14))
        aExpectedColumnDescriptions = ('Column D Column D', 'cc_girder2', 'Column S Column S',
                'Column S Column S', 'Column D Column D', 'clexrfdon', 'Column S Column S',
                'Column S Column S', 'Column D', 'clexrfdoff', 'Column S Column S', 'Column S Column S')

        # Without the fix in place, this test would have failed here,
        # since the pasted chart wouldn't have had any data
        self.assertEqual(aExpectedRowDescriptions, xData.RowDescriptions)
        self.assertEqual(aExpectedColumnDescriptions, xData.ColumnDescriptions)

# vim: set shiftwidth=4 softtabstop=4 expandtab:
