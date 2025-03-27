<?php

// Include the Twilio PHP Helper Library. 
require_once 'twilio-php-main/src/Twilio/autoload.php';
use Twilio\Rest\Client;

// Define the assistant class and its functions:
class assistant {
	public $conn;
	private $twilio;
	
	public function __init__($conn){
		$this->conn = $conn;
		// Define the Twilio account information and object.
		$_sid    = "<_SID_>";
        $token  = "<_TOKEN_>";
        $this->twilio = new Client($_sid, $token);
	}
	
	// Database -> Update the database table
	public function update_user_info($action, $to_phone, $from_phone, $emergency_phone){
		if($action == "new"){
			// Add the given user information to the database table.
			$sql_insert = "INSERT INTO `entries`(`to_phone`, `from_phone`, `emergency_phone`, `date`, `location`, `class`) 
						   VALUES ('$to_phone', '$from_phone', '$emergency_phone', 'X', 'X', 'X')"
					      ;
			if(mysqli_query($this->conn, $sql_insert)) echo("User information added to the database successfully!");			
		}else if($action == "update"){
			// Update the given user information in the database table.
			$sql_update = "UPDATE `entries` SET `to_phone`='$to_phone', `from_phone`='$from_phone', `emergency_phone`='$emergency_phone'
			               WHERE `id`=1"
						  ;
			if(mysqli_query($this->conn, $sql_update)) echo("User information updated successfully!");

		}
	}
	
	// Save the latest model detection results and the device location to the database table.
	public function save_results($action, $date, $location, $class){
		if($action == "save"){
			// Fetch the stored user information to add to the new data record.
			$sql_insert = "INSERT INTO `entries`(`to_phone`, `from_phone`, `emergency_phone`, `date`, `location`, `class`) 
						   SELECT `to_phone`, `from_phone`, `emergency_phone`, '$date', '$location', '$class'
						   FROM `entries` WHERE id=1"
			              ;
			if(mysqli_query($this->conn, $sql_insert)) echo("New data record inserted successfully!");
		}
	}
	
	// Obtain the required user information from the database table. 
	private function get_user_info(){
		$sql = "SELECT * FROM `entries` WHERE id=1";
		$result = mysqli_query($this->conn, $sql);
		$check = mysqli_num_rows($result);
		if($check > 0 && $row = mysqli_fetch_assoc($result)){
			return $row;
		}
	}
	
    // Retrieve the required location variables from the database table.
	private function get_loc_vars(){
		$loc_vars = [];
		$sql = "SELECT * FROM `entries` WHERE id!=1 ORDER BY `id` DESC";
		$result = mysqli_query($this->conn, $sql);
		$check = mysqli_num_rows($result);
		while($check > 0 && $row = mysqli_fetch_assoc($result)){
			array_push($loc_vars, $row);
		}
		return $loc_vars;
	}

	// Transfer a WhatsApp message to the registered primary emergency contact via Twilio.
	private function Twilio_send_whatsapp($body){
		// Get user information.
		$info = $this->get_user_info();
		// Configure the WhatsApp object.
		$whatsapp_message = $this->twilio->messages
			->create("whatsapp:".$info["to_phone"],
				array(
					   "from" => "whatsapp:+14155238886",
					   "body" => $body
                     )
				);
		// Send the WhatsApp message.
        echo(" WhatsApp SID: ".$whatsapp_message->sid);
	}
	
	// Send an SMS to the registered secondary emergency contact via Twilio.
	private function Twilio_send_SMS($body){
		// Get user information.
		$info = $this->get_user_info();
		// Configure the SMS object.
        $sms_message = $this->twilio->messages
			->create($info["emergency_phone"],
				array(
					   "from" => $info["from_phone"],
                       "body" => $body
                     )
                );
		// Send the SMS.
		echo(" SMS SID: ".$sms_message->sid);	  
	}
	
	// Inform the user's emergency contacts via SMS or WhatsApp, depending on the given command.
	public function notify_contacts($command, $date, $location){
		// Decode the received location parameters.
		$loc_vars = explode(",", $location);
		$lat = $loc_vars[0]; $long = $loc_vars[1]; $alt = $loc_vars[2];
		// Notify the emergency contacts.
		if($command == "fine"){
			$message_body = "😄 👍 I am doing well. I just wanted to add this location as a breadcrumb 😄 👍"
			                ."\n\r\n\r⏰ Date: ".$date
							."\n\r\n\r✈️ Altitude: ".$alt
							."\n\r\n\r📌 Current Location:\n\rhttps://www.google.com/maps/search/?api=1&query=".$lat."%2C".$long;
			$this->Twilio_send_whatsapp($message_body);
		}
		else if($command == "danger"){
			$message_body = "⚠️ ⚠️ ⚠️ I do not feel safe and may be in peril ⚠️ ⚠️ ⚠️"
			                ."\n\r\n\r⏰ Date: ".$date
							."\n\r\n\r✈️ Altitude: ".$alt
							."\n\r\n\r📌 Current Location:\n\rhttps://www.google.com/maps/search/?api=1&query=".$lat."%2C".$long;
			$this->Twilio_send_whatsapp($message_body);
		}
		else if($command == "assist"){
			$message_body = "♿ ♿ ♿ I may need your assistance due to restrictive layouts ♿ ♿ ♿"
			                ."\n\r\n\r⏰ Date: ".$date
							."\n\r\n\r✈️ Altitude: ".$alt
							."\n\r\n\r📌 Current Location:\n\rhttps://www.google.com/maps/search/?api=1&query=".$lat."%2C".$long;
			$this->Twilio_send_whatsapp($message_body);
		}
		else if($command == "stolen"){
			$message_body = "💰 👮🏻 💰 Someone managed to purloin my valuables near this location 💰 👮🏻 💰"
			                ."\n\r\n\r⏰ Date: ".$date
							."\n\r\n\r✈️ Altitude: ".$alt
							."\n\r\n\r📌 Current Location:\n\rhttps://www.google.com/maps/search/?api=1&query=".$lat."%2C".$long;
			$this->Twilio_send_whatsapp($message_body);
		}
		else if($command == "call"){
			$message_body = "📞 ☎️ 📞 Please inform my first emergency contact that I am near this location 📞 ☎️ 📞"
			                ."\n\r\n\r⏰ Date: ".$date
							."\n\r\n\r✈️ Altitude: ".$alt
							."\n\r\n\r📌 Current Location:\n\r\n\rhttps://www.google.com/maps/search/?api=1&query=".$lat."%2C".$long;
			$this->Twilio_send_SMS($message_body);
		}
	}
	
	// Transfer thorough location inspections generated by Google Maps to emergency contacts when requested via WhatsApp.
	public function generate_feedback($command){
		// Obtain the latest location variables in the database table.
		$loc_vars = $this->get_loc_vars();
		// Generate the requested feedback via Google Maps.
		$message_body = "";
		switch($command){
			case "Route Walking":
				if(count($loc_vars) >= 2){
					$message_body = "🚶 Estimated Walking Path:\n\r\n\r"
									."📌 Origin ➡️\n\r"
									."🕜 ".$loc_vars[1]["date"]
									."\n\r🔎 ".$loc_vars[1]["class"]
									."\n\r\n\r📌 Destination ➡️\n\r"
									."🕜 ".$loc_vars[0]["date"]
									."\n\r🔎 ".$loc_vars[0]["class"]
									."\n\r\n\rhttps://www.google.com/maps/dir/?api=1&origin="
									.explode(",", $loc_vars[1]["location"])[0]."%2C"
									.explode(",", $loc_vars[1]["location"])[1]."&destination="
									.explode(",", $loc_vars[0]["location"])[0]."%2C"
									.explode(",", $loc_vars[0]["location"])[1]
									."&travelmode=walking";
				}else{
					$message_body = "⛔ Insufficient data for the requested analysis.";
				}
				break;
			case "Route Bicycling":
				if(count($loc_vars) >= 2){
					$message_body = "🚴 Estimated Bicycling Path:\n\r\n\r"
									."📌 Origin ➡️\n\r"
									."🕜 ".$loc_vars[1]["date"]
									."\n\r🔎 ".$loc_vars[1]["class"]
									."\n\r\n\r📌 Destination ➡️\n\r"
									."🕜 ".$loc_vars[0]["date"]
									."\n\r🔎 ".$loc_vars[0]["class"]
									."\n\r\n\rhttps://www.google.com/maps/dir/?api=1&origin="
									.explode(",", $loc_vars[1]["location"])[0]."%2C"
									.explode(",", $loc_vars[1]["location"])[1]."&destination="
									.explode(",", $loc_vars[0]["location"])[0]."%2C"
									.explode(",", $loc_vars[0]["location"])[1]
									."&travelmode=bicycling";
				}else{
					$message_body = "⛔ Insufficient data for the requested analysis.";
				}
				break;
			case "Route Driving":
				if(count($loc_vars) >= 2){
					$message_body = "🚗 Estimated Driving Path:\n\r\n\r"
									."📌 Origin ➡️\n\r"
									."🕜 ".$loc_vars[1]["date"]
									."\n\r🔎 ".$loc_vars[1]["class"]
									."\n\r\n\r📌 Destination ➡️\n\r"
									."🕜 ".$loc_vars[0]["date"]
									."\n\r🔎 ".$loc_vars[0]["class"]
									."\n\r\n\rhttps://www.google.com/maps/dir/?api=1&origin="
									.explode(",", $loc_vars[1]["location"])[0]."%2C"
									.explode(",", $loc_vars[1]["location"])[1]."&destination="
									.explode(",", $loc_vars[0]["location"])[0]."%2C"
									.explode(",", $loc_vars[0]["location"])[1]
									."&travelmode=driving";
				}else{
					$message_body = "⛔ Insufficient data for the requested analysis.";
				}
				break;
			case "Show Waypoints":
				if(count($loc_vars) >= 4){
					$message_body = "🛣️ 📍 Estimated Driving Path w/ Waypoints:\n\r\n\r"
									."📌 Origin ➡️\n\r"
									."🕜 ".$loc_vars[3]["date"]
									."\n\r🔎 ".$loc_vars[3]["class"]
									."\n\r\n\r📌 Destination ➡️\n\r"
									."🕜 ".$loc_vars[0]["date"]
									."\n\r🔎 ".$loc_vars[0]["class"]
									."\n\r\n\rhttps://www.google.com/maps/dir/?api=1&origin="
									.explode(",", $loc_vars[3]["location"])[0]."%2C"
									.explode(",", $loc_vars[3]["location"])[1]."&waypoints="
									.explode(",", $loc_vars[2]["location"])[0]."%2C"
									.explode(",", $loc_vars[2]["location"])[1]."%7C"
									.explode(",", $loc_vars[1]["location"])[0]."%2C"
									.explode(",", $loc_vars[1]["location"])[1]."&destination="
									.explode(",", $loc_vars[0]["location"])[0]."%2C"
									.explode(",", $loc_vars[0]["location"])[1]
									."&travelmode=driving";
				}else{
					$message_body = "⛔ Insufficient data for the requested analysis.";
				}
				break;
			case "Terrain View":
				if(count($loc_vars) >= 1){
					$message_body = "⛰️ Terrain View of the Latest Location:\n\r\n\r"
									."📌 Latest ➡️\n\r"
									."🕜 ".$loc_vars[0]["date"]
									."\n\r🔎 ".$loc_vars[0]["class"]
									."\n\r\n\rhttps://www.google.com/maps/@?api=1&map_action=map&center="
									.explode(",", $loc_vars[0]["location"])[0]."%2C"
									.explode(",", $loc_vars[0]["location"])[1]
									."&zoom=12&basemap=terrain&layer=bicycling";
				}else{
					$message_body = "⛔ Insufficient data for the requested analysis.";
				}
				break;
			case "Satellite View":
				if(count($loc_vars) >= 1){
					$message_body = "🛰️ Satellite View of the Latest Location:\n\r\n\r"
									."📌 Latest ➡️\n\r"
									."🕜 ".$loc_vars[0]["date"]
									."\n\r🔎 ".$loc_vars[0]["class"]
									."\n\r\n\rhttps://www.google.com/maps/@?api=1&map_action=map&center="
									.explode(",", $loc_vars[0]["location"])[0]."%2C"
									.explode(",", $loc_vars[0]["location"])[1]
									."&zoom=12&basemap=satellite&layer=traffic";
				}else{
					$message_body = "⛔ Insufficient data for the requested analysis.";
				}
				break;
			case "Street View":
				if(count($loc_vars) >= 1){
					$message_body = "🚀 🌎 Wander through the streets near the latest location:\n\r\n\r"
									."📌 Center ➡️\n\r"
									."🕜 ".$loc_vars[0]["date"]
									."\n\r🔎 ".$loc_vars[0]["class"]
									."\n\r\n\rhttps://www.google.com/maps/@?api=1&map_action=pano&viewpoint="
									.explode(",", $loc_vars[0]["location"])[0]."%2C"
									.explode(",", $loc_vars[0]["location"])[1]
									."&heading=90";
				}else{
					$message_body = "⛔ Insufficient data for the requested analysis.";
				}
				break;
			default:
				$message_body = "🤖 👉 Please utilize the supported commands:\n\r\n\r➡️ Route Walking\n\r\n\r➡️ Route Bicycling\n\r\n\r➡️ Route Driving\n\r\n\r➡️ Show Waypoints\n\r\n\r➡️ Terrain View\n\r\n\r➡️ Satellite View\n\r\n\r➡️ Street View";
				break;
		}
		// Transmit the generated feedback.
		$this->Twilio_send_whatsapp($message_body);
	}

}

// Define database and server settings:
$server = array(
	"name" => "localhost",
	"username" => "root",
	"password" => "",
	"database" => "emergency_assistant"
);


$conn = mysqli_connect($server["name"], $server["username"], $server["password"], $server["database"]);

?>