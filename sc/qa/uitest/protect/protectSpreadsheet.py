# -*- tab-width: 4; indent-tabs-mode: nil; py-indent-offset: 4 -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
from uitest.framework import UITestCase
from uitest.uihelper.common import get_state_as_dict
from uitest.uihelper.common import select_pos
from uitest.uihelper.calc import enter_text_to_cell
from libreoffice.calc.document import get_sheet_from_doc
from libreoffice.calc.conditional_format import get_conditional_format_from_sheet
from uitest.debug import sleep
from libreoffice.calc.document import get_cell_by_position
from libreoffice.uno.propertyvalue import mkPropertyValues

class protectSpreadsheet(UITestCase):
    def test_protect_spreadsheet(self):
        with self.ui_test.create_doc_in_start_center("calc"):
            xCalcDoc = self.xUITest.getTopFocusWindow()
            gridwin = xCalcDoc.getChild("grid_window")
            #enter password
            with self.ui_test.execute_dialog_through_command(".uno:ToolProtectionDocument") as xDialog:
                xpass1ed = xDialog.getChild("pass1ed")
                xconfirm1ed = xDialog.getChild("confirm1ed")

                xpass1ed.executeAction("TYPE", mkPropertyValues({"TEXT":"aa"}))
                xconfirm1ed.executeAction("TYPE", mkPropertyValues({"TEXT":"aa"}))

            #Verify
            with self.ui_test.execute_dialog_through_command(".uno:ToolProtectionDocument", close_button="cancel") as xDialog:
                xpass1ed = xDialog.getChild("pass1ed")

                xpass1ed.executeAction("TYPE", mkPropertyValues({"TEXT":"aa"}))



# vim: set shiftwidth=4 softtabstop=4 expandtab:
