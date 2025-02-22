/*
    SPDX-FileCopyrightText: 2018-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2018-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "test_utils.hpp"
#define private public
#define protected public

#include "bin/binplaylist.hpp"
#include "doc/kdenlivedoc.h"

using namespace fakeit;

TEST_CASE("Test of timewarping", "[Timewarp]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Create document
    KdenliveDoc document(undoStack, {0, 2});
    Mock<KdenliveDoc> docMock(document);
    KdenliveDoc &mockedDoc = docMock.get();
    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    mocked.m_project = &mockedDoc;
    QDateTime documentDate = QDateTime::currentDateTime();
    mocked.updateTimeline(0, false, QString(), QString(), documentDate, 0);
    auto timeline = mockedDoc.getTimeline(mockedDoc.uuid());
    mocked.m_activeTimelineModel = timeline;
    mocked.testSetActiveDocument(&mockedDoc, timeline);

    TimelineModel::next_id = 0;

    QString binId = createProducer(*timeline->getProfile(), "red", binModel);
    QString binId2 = createProducer(*timeline->getProfile(), "blue", binModel);
    QString binId3 = createProducerWithSound(*timeline->getProfile(), binModel);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid1 = timeline->getTrackIndexFromPosition(1);
    int tid2 = timeline->getTrackIndexFromPosition(0);
    Q_UNUSED(tid1);
    Q_UNUSED(tid2);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid3 = ClipModel::construct(timeline, binId3, -1, PlaylistState::VideoOnly);

    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;
    timeline->m_allClips[cid3]->m_endlessResize = false;

    SECTION("Timewarping orphan clip")
    {
        int originalDuration = timeline->getClipPlaytime(cid3);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid3) == -1);
        REQUIRE(timeline->getClipSpeed(cid3) == 1.);

        std::function<bool(void)> undo = []() { return true; };
        std::function<bool(void)> redo = []() { return true; };

        REQUIRE(timeline->requestClipTimeWarp(cid3, 0.1, false, true, undo, redo));

        REQUIRE(timeline->getClipSpeed(cid3) == 0.1);
        INFO(timeline->m_allClips[cid3]->getIn());
        INFO(timeline->m_allClips[cid3]->getOut());
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration / 0.1);

        undo();

        REQUIRE(timeline->getClipSpeed(cid3) == 1.);
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration);

        redo();

        REQUIRE(timeline->getClipSpeed(cid3) == 0.1);
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration / 0.1);

        std::function<bool(void)> undo2 = []() { return true; };
        std::function<bool(void)> redo2 = []() { return true; };
        REQUIRE(timeline->requestClipTimeWarp(cid3, 1.2, false, true, undo2, redo2));

        REQUIRE(timeline->getClipSpeed(cid3) == 1.2);
        REQUIRE(timeline->getClipPlaytime(cid3) == int(originalDuration / 1.2));

        undo2();
        REQUIRE(timeline->getClipSpeed(cid3) == 0.1);
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration / 0.1);

        undo();
        REQUIRE(timeline->getClipSpeed(cid3) == 1.);
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration);

        // Finally, we test that setting a very high speed isn't possible.
        // Specifically, it must be impossible to make the clip shorter than one frame
        int curLength = timeline->getClipPlaytime(cid3);

        // This is the limit, should work
        REQUIRE(timeline->requestClipTimeWarp(cid3, double(curLength), false, true, undo2, redo2));

        REQUIRE(timeline->getClipSpeed(cid3) == double(curLength));
        REQUIRE(timeline->getClipPlaytime(cid3) == 1);

        // This is the higher than the limit, should not work
        // (we have some error margin in duration rounding, multiply by 10)
        REQUIRE_FALSE(timeline->requestClipTimeWarp(cid3, double(curLength) * 10, false, true, undo2, redo2));
    }
    binModel->clean();
    undoStack->clear();
    pCore->m_projectManager = nullptr;
}
