-- phpMyAdmin SQL Dump
-- version 5.2.0
-- https://www.phpmyadmin.net/
--
-- 主机： localhost
-- 生成日期： 2022-10-05 10:10:02
-- 服务器版本： 5.7.37-log
-- PHP 版本： 7.4.30

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- 数据库： `xiuxian`
--

-- --------------------------------------------------------

--
-- 表的结构 `chuangdang`
--

CREATE TABLE `chuangdang` (
  `ziznegid` int(11) NOT NULL,
  `uuid` varchar(255) NOT NULL,
  `uname` varchar(255) NOT NULL,
  `jianggong` varchar(255) NOT NULL,
  `jifen` varchar(255) NOT NULL,
  `updt` datetime(6) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- 表的结构 `dict_jingjie`
--

CREATE TABLE `dict_jingjie` (
  `jingjie_id` varchar(11) NOT NULL,
  `jingjie_str` varchar(255) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- 转存表中的数据 `dict_jingjie`
--

INSERT INTO `dict_jingjie` (`jingjie_id`, `jingjie_str`) VALUES
('0', '炼体期'),
('1', '练气期'),
('2', '筑基期'),
('3', '金丹期'),
('4', '元婴期'),
('5', '化神期'),
('6', '炼虚期'),
('7', '合体期'),
('8', '大乘期'),
('9', '渡劫期');

-- --------------------------------------------------------

--
-- 表的结构 `feedback`
--

CREATE TABLE `feedback` (
  `id` int(11) NOT NULL,
  `info` varchar(255) NOT NULL,
  `up_dt` datetime(6) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- 表的结构 `xiuxian`
--

CREATE TABLE `xiuxian` (
  `id` int(11) NOT NULL,
  `uuid` varchar(255) NOT NULL,
  `user_name` varchar(255) NOT NULL,
  `sheng_ming` varchar(255) NOT NULL,
  `gong_ji` varchar(255) NOT NULL,
  `fang_yu` varchar(255) NOT NULL,
  `wu_xing` varchar(255) NOT NULL,
  `xiu_wei` varchar(255) NOT NULL,
  `jing_jie` varchar(255) NOT NULL,
  `remote_addr` varchar(255) NOT NULL,
  `up_dt` datetime(6) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- 转储表的索引
--

--
-- 表的索引 `chuangdang`
--
ALTER TABLE `chuangdang`
  ADD PRIMARY KEY (`ziznegid`);

--
-- 表的索引 `feedback`
--
ALTER TABLE `feedback`
  ADD PRIMARY KEY (`id`);

--
-- 表的索引 `xiuxian`
--
ALTER TABLE `xiuxian`
  ADD PRIMARY KEY (`id`);

--
-- 在导出的表使用AUTO_INCREMENT
--

--
-- 使用表AUTO_INCREMENT `chuangdang`
--
ALTER TABLE `chuangdang`
  MODIFY `ziznegid` int(11) NOT NULL AUTO_INCREMENT;

--
-- 使用表AUTO_INCREMENT `feedback`
--
ALTER TABLE `feedback`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- 使用表AUTO_INCREMENT `xiuxian`
--
ALTER TABLE `xiuxian`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=119;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
