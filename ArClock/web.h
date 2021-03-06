/*
 * ArClock
 * 
 * (c) Matt Aubury 2020
 */
 
#pragma once

#include "page_template.h"
#include "page_mqtt_template.h"
#include "settings.h"
#include "clock_fonts.h"
#include "effect.h"
#include "color.h"
#include "message.h"

/**************************************************************************/

/*
 * Used to make a list of options 
 */
template<typename List>
String make_options (const List &list, const String &current)
{
  String choices;
  for (const auto &item : list)
  {
    choices += F(R""(<option value=")"");
    choices += item;
    choices += '"';
    if (String (FPSTR(item)) == current)
    {
      choices += F( " selected");
    }
    choices += ">";
    choices += item;
    choices += F(R""(</option>)"");
  }
  return choices;
}

/**************************************************************************/

std::size_t currentSequenceHash = 0;

void apply () 
{
  /*
   * Apply them (many settings are applied dynamically)
   */
  //useSequence = settings.at (F("useSequence"))=="checked";
  autoBright = settings.at (F("autoBrightness"))=="checked";
  if(autoBright) {
    // change automaticly
  } else {
    
    strip.SetBrightness (settings.at (F("masterBrightness")).toInt ());
  }
  pinger.StopPingSequence ();
  if (settings.at (F("pingTarget")).length () > 1)
  {
    pinger.Ping (settings.at (F("pingTarget")), 4000000000);
  }

  const char * newSequence = settings.at (F("sequence")).c_str ();
  std::size_t newHash = std::hash<std::string>{}(newSequence);
  
  if(currentSequenceHash != newHash) {
    sequence.clear();
    current_seq = 0;
    parse_sequence(newSequence);
  }
  
  currentSequenceHash = newHash;
  /*
   * Configure NTP for the correct timezone
   * 
   * NB: linear search because otherwise the std::map is too large
   */
  const auto timezone = settings.at (F("timezone")).c_str ();
  for (size_t i = 0, end = std::end (timezone_names) - std::begin (timezone_names); i != end; ++i)
  {
    if (::strcmp (timezone, timezone_names[i]) == 0)
    {
      configTime (timezone_values[i], "pool.ntp.org");
    }
  }
}

/*
 * Called when a change happens to the web form
 */
void update () 
{
  /*
   * Read updated settings from web server
   */
  for (auto &key_value : settings)
  {
    auto &key = key_value.first;
    auto &value = key_value.second;
    if (server.hasArg (key))
    {
      value = server.arg (key);
      Serial.print(key);
      Serial.print("->");
      Serial.println(value);
    }
  }

  apply();
}

/**************************************************************************/

void replaceVariables(String& result) {

  for (const auto &setting : settings)
  { 
    result.replace ("{{" + setting.first + "}}", setting.second);
  }

  /*
   * Insert other dynamic data
   */
  result.replace (F("{{PanelWidth}}"), String (PanelWidth));
  result.replace (F("{{PanelHeight}}"), String (PanelHeight));
  result.replace (F("{{preset_choices}}"), make_options (preset_names, ""));
  result.replace (F("{{primary_font_choices}}"), make_options (font_names, settings.at (F("pFt"))));
  result.replace (F("{{primary_color_mode_choices}}"), make_options (color_mode_names, settings.at (F("pCM"))));
  result.replace (F("{{secondary_font_choices}}"), make_options (font_names, settings.at (F("sFt"))));
  result.replace (F("{{secondary_color_mode_choices}}"), make_options (color_mode_names, settings.at (F("sCM"))));
  result.replace (F("{{effect_choices}}"), make_options (effect_names, settings.at (F("effect"))));
  result.replace (F("{{effect_color_mode_choices}}"), make_options (color_mode_names, settings.at (F("effectColorMode"))));
  result.replace (F("{{hide_net}}"), settings.at (F("ssid"))==""?"":"hide");

  int i = 0;
  for (const auto &value : qValues)
  { 
    i++;
    result.replace ("{{v" + String(i) + "}}", value);
  }

}

/*
 * Default webpage load
 */
void handle_root ()
{
  /*
   * Replace all settings in the template with the correct values
   * 
   * Note: this operation is memory constrained, so we pre-reserve the
   * string to avoid running out of memory. If you make the page much
   * bigger you'll be in trouble though...
   */
  String result;
  result.reserve (10000); 
  result += FPSTR (page_template);

  replaceVariables(result);

  /*   
   * Send the completed page
   */
  server.sendHeader ("cache-control", "max-age=10");
  server.send (200, F("text/html"), result);
}

/*
 * MQTT sensors config webpage load
 */
void handle_mqtt ()
{
  String result;
  result.reserve (10000);
  result += FPSTR (page_mqtt_template);

  replaceVariables(result);

  /*   
   * Send the completed page
   */
  server.sendHeader ("cache-control", "max-age=10");
  server.send (200, F("text/html"), result);
}

/**************************************************************************/

/*
 * XMLHttpRequest dynamic update (e.g. with sliders)
 */
void handle_update () 
{
  update ();
  server.send (200);
}

/**************************************************************************/

/*
 * XMLHttpRequest full update (form value change)
 */
void handle_change () 
{
  update ();
  server.send (200);
  save_settings ();
}

void handle_save () 
{
  server.send (200);
  save_settings ();
}

/**************************************************************************/

/*
 * Preset change
 */
void handle_load_preset ()
{
  load_preset (server.arg ("preset"));
  apply();
  server.send (200);
}

/**************************************************************************/

/*
 * Reboot the clock
 */
const char reboot_template[] PROGMEM = 
R""(
<!DOCTYPE html>
<html>
   <body>
      <script>
         setTimeout(function(){
            window.location.href = 'http://{{IPLocal}}';
         }, 8000);
      </script>
      <p>Rebooting...</p>
   </body>
</html>)"";
 
void handle_reboot ()
{
  String result;
  result.reserve (3000); 
  result += FPSTR (reboot_template);
  result.replace (F("{{IPLocal}}"), WiFi.localIP ().toString());
  server.send (200, F("text/html"), result);
  
  delay (500);
  ESP.restart ();
}

/**************************************************************************/

/*
 * XMLHttpRequest to get a list of timezones
 */
void handle_timezones () 
{
  String result = "[";
  for (auto it = std::begin (timezone_names); it != std::end (timezone_names); ++it)
  {
    result += '"';
    result += *it;
    result += '"';
    if (std::next (it) != std::end (timezone_names))
    {
      result += ",\n";
    }
  }
  result += "]";
  server.send (200, F("application/json"), result);
}

/**************************************************************************/

/*
 * Display a user message
 */
void handle_show ()
{
  if (server.hasArg (F("message")))
  {
    settings[F("message")] = server.arg (F("message"));
    auto interval = settings[F("messageRepeat")].toInt ();
    next_repeat = millis () + (interval * 1000); 
    show_message (server.arg (F("message")));
  }
  server.send (200);
}

/**************************************************************************/

void handle_part ()
{
  String part = server.arg (F("p"));
  String result;
  result.reserve (5000);
  if(part == "conn") {
    result += FPSTR (page_template_connection);
    Serial.println ("page_template_connection");
  } 
  else if(part == "master")
  {
    result += FPSTR (page_template_master);
    Serial.println ("page_template_master");
  }
  else if(part == "effects")
  {
    result += FPSTR (page_template_effects);
    Serial.println ("page_template_effects");
  }
  else if(part == "weather")
  {
    result += FPSTR (page_template_weather);
    Serial.println ("page_template_weather");
  }
  else if(part == "message")
  {
    result += FPSTR (page_template_message);
    Serial.println ("page_template_message");
  }

  replaceVariables(result);
  /*   
   * Send the completed page
   */
  server.sendHeader ("cache-control", "max-age=10");
  server.send (200, F("text/html"), result);
}