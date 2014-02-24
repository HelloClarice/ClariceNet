<?php

parse_str($_SERVER['QUERY_STRING']);
$val = isset($name) ? $name : 'sensor1';

// extract last timestamp update
$timestamp = trim(shell_exec('rrdtool last /home/pi/clarice/clarice.rrd'));
//print_r($timestamp);
$str = "rrdtool xport -s {$timestamp}-3s -e {$timestamp}-3s DEF:a=/home/pi/clarice/clarice.rrd:{$val}:AVERAGE XPORT:a";
// print_r($str);
$output = trim(shell_exec($str));
$xml = simplexml_load_string($output);

$data = array( "data" => array(
	"name" => $val,
	"t" => (string)$xml->data->row[0]->t,
	"value" => (string)$xml->data->row[0]->v
));

header("Content-Type: application/json");
print_r(json_encode($data));

?>
