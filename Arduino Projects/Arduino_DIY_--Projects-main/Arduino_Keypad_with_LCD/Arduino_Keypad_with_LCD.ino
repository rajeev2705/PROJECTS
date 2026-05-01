#include <LiquidCrystal.h>
#include <Keypad.h>
// #include <LiquidCrystal_I2C.h>

// Initialize the LCD
LiquidCrystal lcd(3, 2, 9, 10, 11, 12);
// LiquidCrystal_I2C lcd(0x27,16,2);

// Define the keys present on the keypad
char keys[4][4] = {
  {'7', '8', '9', '/'},
  {'4', '5', '6', '*'},
  {'1', '2', '3', '-'},
  {'c', '0', '=', '+'}
};

// Specify the pins used for the rows and columns of the keypad
byte rowPins[] = {A3, A2, A1, A0};
byte colPins[] =  {4, 5, 6, 7};

// Initialize the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, 4, 4);

// Variables for input handling and calculations
String takeInputasString = ""; // Increased size to handle larger numbers and prevent overflow
long result = 0;

void setup() {
  lcd.begin(16, 2); // Initialize the LCD
  // lcd.init();
  lcd.clear();      // Clear the LCD
}

void loop() {
  char input = keypad.getKey(); // Read the input from the keypad

  if (input) {
    // Check for clear operation
    if (input == 'c') {
      // Clear the input and reset the calculator
      takeInputasString = "";
      result = 0;
      lcd.clear();
    } 
    // Check for equals operation
    else if (input == '=') {
      // Perform the calculation and display the result
      result = calculateExpression(takeInputasString);
      lcd.setCursor(0, 1);
      lcd.print(result);

      // Reset the input string to the result
      takeInputasString = String(result);
    } 
    // Numeric input or operator
    else {
      // Append the current numeric input or '.' to the input string
      takeInputasString += input;
      lcd.clear();
      lcd.print(takeInputasString); // Display the current input string
    }
  }
}

// Function for expression calculation
int calculateExpression(String expression) {
  int result = 0;
  int operand = 0;
  char operation = '+';
  bool validExpression = true;

  // Loop through each character in the expression
  for (size_t i = 0; i < expression.length(); ++i) {
    char currentChar = expression.charAt(i);

    // Check if the character is a digit
    if (isdigit(currentChar)) {
      operand = operand * 10 + (currentChar - '0');
    } 
    // Check if the character is an operator
    else if (currentChar == '+' || currentChar == '-' || currentChar == '*' || currentChar == '/') {
      // Check for consecutive operators which indicate an invalid expression
      if (i > 0 && (expression.charAt(i - 1) == '+' || expression.charAt(i - 1) == '-' ||
                    expression.charAt(i - 1) == '*' || expression.charAt(i - 1) == '/')) {
        validExpression = false;
        break;  // Exit the loop for an invalid expression
      }

      // Perform the operation based on the current operator
      result = performOperation(result, operand, operation);
      operand = 0;
      operation = currentChar;
    }
  }

  // Perform the last operation if the expression is valid
  if (validExpression) {
    result = performOperation(result, operand, operation);
  } else {
    // Print an error message for an invalid expression
    Serial.println("Invalid expression");
  }

  return result;
}

// Function to perform a basic arithmetic operation
int performOperation(int result, int operand, char operation) {
  switch (operation) {
    case '+':
      return result + operand;
    case '-':
      return result - operand;
    case '*':
      return result * operand;
    case '/':
      if (operand != 0) {
        return result / operand;
      } else {
        // Handle division by zero
        Serial.println("Division by zero");
        return 0;
      }
    default:
      return operand; // Default case for the first operand
  }
}
