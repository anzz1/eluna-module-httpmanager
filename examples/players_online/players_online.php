<?php

// Use this is you want to print the "Last updated" in certain timezone
//date_default_timezone_set('Europe/Helsinki');

// Set this to your secret password for updating the online count
// Put it also in players_online.lua
$secret = "5ecret";

if (isset($_POST["s"]) && isset($_POST["c"])) {
  $s = $_POST["s"];
  $c = $_POST["c"];
  if ($s == $secret && is_numeric($c) && $c >= 0 && $c <= 99999) {
      file_put_contents('online.txt', $c);
      exit();
  }
}


if (file_exists('online.txt')) {
  $online = file_get_contents('online.txt');
  $modified = filemtime('online.txt');
}

?>

<!DOCTYPE html>
<html>
<head>
<title>Players Online</title>
<meta http-equiv="Content-Type" content="text/html;charset=utf-8" />
</head>
<body>
<p>Players online: <?php if (isset($online)) echo $online; ?></p>
<p>Last updated: <?php if (isset($modified)) echo date("D, d M Y H:i:s (T)", $modified); ?></p>
</body>
</html>
