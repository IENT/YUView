TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = helper \
          filesource \
          statistics \
          video
