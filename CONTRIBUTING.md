# How to contribute

Issues on the github site are intended to discuss code problems, crashes and application enhancements.  If you are having an issue with the setup, 
configuration or use of Motion, we have the following additional resources which are better suited to meet these needs.

  * User guide:  [Motion User Guide](https://motion-project.github.io/motion_guide.html)
  * User Group List:  Please sign-up and send your issue to the list [Motion User](https://lists.sourceforge.net/lists/listinfo/motion-user)  
  * IRC:  [#motion](irc://chat.freenode.net/motion) on freenode

It is very important to use these resources for non-code issues since it engages a much wider audience who may have experience resolving
the particular issue you are trying to resolve.  

##  Submitting Problems

Before submitting a issue, please make sure that you are using either the latest release as posted 
[here](https://github.com/Motion-Project/motion/releases) or that you have built the latest source code 
from the github master branch.

If the issue still remains with the current version, please validate that the issue has not already been reported
by searching through the issue log.  

Next, we must be provided the following in order to replicate and ultimately resolve the issue:

  * A complete Motion log for a single run from startup to shutdown at the INF/7 log level.
  * The expected versus actual result

The preferred method of providing the log file is by posting it on [gist](https://gist.github.com/).  Only provide
the link to the gist file within the issue. The full configuration will be printed out to the log at the INF/7 level
with the most common, sensitive information (URLs, usernames/passwords, etc) masked. It is recommended that you
double check before posting the log file.
For more information please read [privacy wiki article](https://github.com/Motion-Project/motion/wiki/Privacy)

Note that the developers do not use any front-end application to use Motion and we need the actual logs from the Motion
application rather than logs from the front-end application.


##  Submitting an Enhancement Request

Motion has a extremely large number of configuration options.  With so many options, it is important to include a description
of how/why the enhancement will be used.  It is possible that the existing options can be configured to address the need.  
(Which could then lead to a different enhancement than originally contemplated of making those options easier to use or documented
better.)


## Submitting changes

Generally, it is best to first submit a issue on the particular enhancement prior to a pull request.  This allows the particular
item to be discussed and determine how it would fit into the application.

As pull requests are prepared, in addition to the actual code, please also consider:

  * Changes needed to the Motion_Guide.html which is our user guide.
  * Changes to the motion.1 file which is the manual
  * Changes to the configuration templates of motion.conf, camera1.conf, etc.


Thanks,
Motion-Project Team.
