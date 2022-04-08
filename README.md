# qt_qa_engine

## Quick Engine specific functions

### app:waitForPropertyChange

synchronously wait for element property value change

Usage:

`driver.execute_script("app:waitForPropertyChange", "ContextMenu_0x12345678", "opened", true, 10000)`

`"ContextMenu_0x12345678"` is element.id, you should find element before using this method

You can use None as property value to wait for any property change, or exact value to watch for.

10000 - is timeout to wait for change or continue anyway

### touch:pressAndHold (deprecated, use TouchAction instead)

perform press and hold touch action on choosen coordinates

Usage:

`driver.execute_script("touch:pressAndHold", 20, 40)`

### touch:mouseSwipe (deprecated, use TouchAction instead)

perform mouse swipe touch action from one point to another

Usage:

`driver.execute_script("touch:mouseSwipe", 20, 40, 60, 80)`

### touch:mouseDrag (deprecated, use TouchAction instead)

perform mouse drag touch action from one point to another

Usage:

`driver.execute_script("touch:mouseDrag", 20, 40, 60, 80)`

### app:method

invoke meta method on element

Usage:

`driver.execute_script("app:method", "MyItem_0x12345678", "myFunction", ["some", "args", 15])`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:js

execute js code in element context

Usage:

`driver.execute_script("app:js", "MyItem_0x12345678", "function() { return "hello!"; }"`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:setAttribute

set attribute value in element

Usage:

`driver.execute_script("app:setAttribute", "MyItem_0x12345678", "attribute_name", "value")`

`"MyItem_0x12345678"` is element.id, you should find element before using this method


## Qt Widgets specific execute_script methods list

### app:dumpInView

List elements in view

Usage:

`driver.execute_script("app:dumpInView", "MyItem_0x12345678")`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:posInView

Returns center coordinates of element item in view

Usage:

`driver.execute_script("app:posInView", "MyItem_0x12345678", "ElementName")`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:clickInView

Click center coordinates of element item in view

Usage:

`driver.execute_script("app:clickInView", "MyItem_0x12345678", "ElementName")`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:scrollInView

Scroll view to show element

Usage:

`driver.execute_script("app:scrollInView", "MyItem_0x12345678", "ElementName")`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:dumpInMenu

List all menu elements

Usage:

`driver.execute_script("app:dumpInMenu")`

### app:triggerInMenu

Trigger menu item

Usage:

`driver.execute_script("app:triggerInMenu", "MenuItemName")`

### app:dumpInComboBox

List elements in ComboBox

Usage:

`driver.execute_script("app:dumpInComboBox", "MyItem_0x12345678")`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:activateInComboBox

Activates ComboBox element

Usage:

`driver.execute_script("app:activateInComboBox", "MyItem_0x12345678", "ElementName")`

or by index (starts from 0)

`driver.execute_script("app:activateInComboBox", "MyItem_0x12345678", 1)`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:dumpInTabBar

List elements in TabBar

Usage:

`driver.execute_script("app:dumpInTabBar", "MyItem_0x12345678")`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:posInTabBar

Returns center coordinates of element item in TabBar

Usage:

`driver.execute_script("app:posInTabBar", "MyItem_0x12345678", "ElementName")`

or by index (starts from 0)

`driver.execute_script("app:posInTabBar", "MyItem_0x12345678", 1`

`"MyItem_0x12345678"` is element.id, you should find element before using this method

### app:activateInTabBar

Activates TabBar element

Usage:

`driver.execute_script("app:activateInTabBar", "MyItem_0x12345678", "ElementName")`

or by index (starts from 0)

`driver.execute_script("app:activateInTabBar", "MyItem_0x12345678", 1)`

`"MyItem_0x12345678"` is element.id, you should find element before using this method
