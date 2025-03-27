<?php

include_once "assets/class.php";

ini_set('display_errors',1);

// Define the new '_assistant' object:
$_assistant = new assistant();
$_assistant->__init__($conn);


// If requested, add or update the user information.
if(isset($_GET["action"]) && isset($_GET["to_phone"]) && isset($_GET["from_phone"]) && isset($_GET["emergency_phone"])){
	$_assistant->update_user_info($_GET["action"], $_GET["to_phone"], $_GET["from_phone"], $_GET["emergency_phone"]);
}

// Obtain the emergency class detected by XIAO ESP32S3 and the current location data through the BLE Android application.
if(isset($_GET["action"]) && isset($_GET["date"]) && isset($_GET["location"]) && isset($_GET["class"])){
	// Insert the latest data record into the database table.
	$_assistant->save_results($_GET["action"], $_GET["date"], $_GET["location"], $_GET["class"]);
	// Notify the user's emergency contacts via SMS or WhatsApp, depending on the received class (command).
	$_assistant->notify_contacts($_GET["class"], $_GET["date"], $_GET["location"]);
}

// If the primary emergency contact transfers a message (command) to this webhook via WhatsApp:
if(isset($_POST["Body"])){
	// Generate and transfer thorough location inspections (from Google Maps) as feedback, depending on the given command.
	$_assistant->generate_feedback($_POST["Body"]);
}

?>