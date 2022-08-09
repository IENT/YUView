TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = StatisticsFileCSVTest.pro \
          StatisticsFileVTMBMSTest.pro \
          StatisticsDataTest.pro
