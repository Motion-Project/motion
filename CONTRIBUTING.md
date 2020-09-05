# How to contribute

Issues on the github site are intended to discuss code problems, crashes and application enhancements.  If you are having an issue with the setup,
configuration or use of MotionPlus, we have the following additional resources which are better suited to meet these needs.

##  Submitting Problems

Provide the following in order to replicate and ultimately resolve the issue:

  * A complete MotionPlus log for a single run from startup to shutdown at the INF/7 log level.
  * The expected versus actual result

The preferred method of providing the log file is by posting it on [gist](https://gist.github.com/).  Only provide
the link to the gist file within the issue. The full configuration will be printed out to the log at the INF/7 level
with the most common, sensitive information (URLs, usernames/passwords, etc) masked. It is recommended that you
double check before posting the log file.

Note that the developers do not use any front-end application to use MotionPlus and we need the actual logs from the MotionPlus application rather than logs from the front-end application.


##  Submitting an Enhancement Request

MotionPlus has a extremely large number of configuration options.  With so many options, it is important to include a description
of how/why the enhancement will be used.  It is possible that the existing options can be configured to address the need.
(Which could then lead to a different enhancement than originally contemplated of making those options easier to use or documented
better.)


## Submitting changes

Generally, it is best to first submit a issue on the particular enhancement prior to a pull request.  This allows the particular
item to be discussed and determine how it would fit into the application.

As pull requests are prepared, in addition to the actual code, please also consider:

  * Changes needed to the motionplus_guide.html which is our user guide.
  * Changes to the motionplus.1 file which is the manual
  * Changes to the configuration templates of motionplus.conf, camera1.conf, etc.


Thanks